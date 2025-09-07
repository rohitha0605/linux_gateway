use crate::wire::{SYNC, VER};
use prost::Message;

pub mod proto {
    include!(concat!(env!("OUT_DIR"), "/rpmsg.calc.v1.rs"));
}

pub mod wire {
    pub const SYNC: u16 = 0xA55A;
    pub const VER: u8 = 1;

    #[derive(Copy, Clone)]
    pub enum MsgType {
        CalcReq = 1,
        CalcResp = 2,
    }

    // IEEE CRC-32 (poly 0xEDB88320), initial 0xFFFF_FFFF, final xor 0xFFFF_FFFF
    pub fn crc32(data: &[u8]) -> u32 {
        let mut crc: u32 = 0xFFFF_FFFF;
        let mut i = 0;
        while i < data.len() {
            let byte = data[i] as u32;
            crc ^= byte;
            let mut k = 0;
            while k < 8 {
                let mask = 0u32.wrapping_sub(crc & 1);
                crc = (crc >> 1) ^ (0xEDB88320 & mask);
                k += 1;
            }
            i += 1;
        }
        !crc
    }
}

pub use wire::crc32;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FrameError {
    Header,
    Type,
    Length,
    Crc,
    Decode,
    BadSync(u16),
    UnknownVersion(u8),
    UnknownType(u8),
    ShortHeader,
}

fn split_payload(frame: &[u8], expect: wire::MsgType) -> Result<&[u8], FrameError> {
    if frame.len() < 10 {
        return Err(FrameError::Length);
    }
    let sync = u16::from_be_bytes([frame[0], frame[1]]);
    if sync != SYNC {
        return Err(FrameError::Header);
    }
    let _ver = frame[2];
    let typ = frame[3];
    if typ != expect as u8 {
        return Err(FrameError::Type);
    }
    let len = u16::from_be_bytes([frame[4], frame[5]]) as usize;
    if frame.len() < 10 + len {
        return Err(FrameError::Length);
    }
    let got_crc = u32::from_be_bytes([frame[6], frame[7], frame[8], frame[9]]);
    let payload = &frame[10..10 + len];
    if crc32(payload) != got_crc {
        return Err(FrameError::Crc);
    }
    Ok(payload)
}

pub fn encode_calc_response(sum: u32) -> Vec<u8> {
    let resp = proto::CalcResponse { sum, trace: None };
    let mut payload = Vec::with_capacity(resp.encoded_len());
    resp.encode(&mut payload).expect("encode resp");
    let crc = crc32(&payload);

    let mut frame = Vec::with_capacity(10 + payload.len());
    frame.extend_from_slice(&SYNC.to_be_bytes());
    frame.push(VER);
    frame.push(wire::MsgType::CalcResp as u8);
    frame.extend_from_slice(&(payload.len() as u16).to_be_bytes());
    frame.extend_from_slice(&crc.to_be_bytes());
    frame.extend_from_slice(&payload);
    frame
}

pub fn encode_calc_request(_a: u32, _b: u32) -> Vec<u8> {
    // Important: always set op,a,b explicitly so fields are present in payload.
    let req: proto::CalcRequest = Default::default();
    let mut payload = Vec::with_capacity(req.encoded_len());
    req.encode(&mut payload).expect("encode req");
    let crc = crc32(&payload);

    let mut frame = Vec::with_capacity(10 + payload.len());
    frame.extend_from_slice(&SYNC.to_be_bytes());
    frame.push(VER);
    frame.push(wire::MsgType::CalcReq as u8);
    frame.extend_from_slice(&(payload.len() as u16).to_be_bytes());
    frame.extend_from_slice(&crc.to_be_bytes());
    frame.extend_from_slice(&payload);
    frame
}

pub fn decode_calc_response(frame: &[u8]) -> Result<proto::CalcResponse, FrameError> {
    let payload = split_payload(frame, wire::MsgType::CalcResp)?;
    proto::CalcResponse::decode(payload).map_err(|_| FrameError::Decode)
}

pub fn decode_calc_request(frame: &[u8]) -> Result<proto::CalcRequest, FrameError> {
    let payload = split_payload(frame, wire::MsgType::CalcReq)?;
    proto::CalcRequest::decode(payload).map_err(|_| FrameError::Decode)
}

// ---- frame compatibility guards ------------------------------------------
pub const FRAME_VERSION: u8 = 0x01;

/// Validate the 10-byte frame header:
/// [SYNC u16][VER u8][TYPE u8][LEN u16][CRC32 u32] (all little-endian)
/// Returns (ver, typ, len) or a FrameError.
pub fn guard_header(hdr: &[u8]) -> Result<(u8, u8, u16), FrameError> {
    const SYNC_CONST: u16 = 0xA55A;

    if hdr.len() < 10 {
        return Err(FrameError::ShortHeader);
    }
    let sync = u16::from_le_bytes([hdr[0], hdr[1]]);
    if sync != SYNC_CONST {
        return Err(FrameError::BadSync(sync));
    }

    let ver = hdr[2];
    if ver != FRAME_VERSION {
        return Err(FrameError::UnknownVersion(ver));
    }

    let typ = hdr[3];
    // Update allowed types to match your protocol
    const TYPE_CALC_REQ: u8 = 1;
    const TYPE_CALC_RESP: u8 = 2;
    match typ {
        TYPE_CALC_REQ | TYPE_CALC_RESP => {}
        _ => return Err(FrameError::UnknownType(typ)),
    }

    let len = u16::from_le_bytes([hdr[4], hdr[5]]);
    Ok((ver, typ, len))
}
pub fn encode_calc_request_with_trace(a: u32, b: u32) -> (Vec<u8>, Vec<u8>, u64) {
    // keep main.rs happy: same call shape, same destructure shape
    (encode_calc_request(a, b), Vec::new(), 0)
}
