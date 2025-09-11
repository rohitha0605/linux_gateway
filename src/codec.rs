use crc32fast::Hasher;
use prost::Message;

use crate::proto::{CalcRequest, CalcResponse, TraceCtx};

#[derive(thiserror::Error, Debug)]
pub enum DecodeError {
    #[error("TooShort")]
    TooShort,
    #[error("UnknownVersion({0})")]
    UnknownVersion(u8),
    #[error("UnknownOpcode({0})")]
    UnknownOpcode(u8),
    #[error("Crc")]
    Crc,
    #[error(transparent)]
    Proto(#[from] prost::DecodeError),
}

const VER: u8 = 0x01;
const OPC_REQ: u8 = 0x01;
const OPC_RESP: u8 = 0x02;

fn frame(opc: u8, payload: &[u8]) -> Vec<u8> {
    // [VER][OPC][payload...][CRC32 little-endian over VER..payload]
    let mut v = Vec::with_capacity(2 + payload.len() + 4);
    v.push(VER);
    v.push(opc);
    v.extend_from_slice(payload);

    let mut h = Hasher::new();
    h.update(&v);
    let crc = h.finalize().to_le_bytes();
    v.extend_from_slice(&crc);
    v
}

fn split_and_check(data: &[u8]) -> Result<(&[u8], u8, u8), DecodeError> {
    if data.len() < 2 + 4 {
        return Err(DecodeError::TooShort);
    }
    let (head_plus_payload, crc_bytes) = data.split_at(data.len() - 4);

    let mut h = Hasher::new();
    h.update(head_plus_payload);
    let want = h.finalize();
    let got = u32::from_le_bytes([crc_bytes[0], crc_bytes[1], crc_bytes[2], crc_bytes[3]]);
    if want != got {
        return Err(DecodeError::Crc);
    }

    let ver = head_plus_payload[0];
    let opc = head_plus_payload[1];
    if ver != VER {
        return Err(DecodeError::UnknownVersion(ver));
    }
    Ok((&head_plus_payload[2..], ver, opc))
}

// ---------- API used by main/examples ----------

pub fn encode_calc_request(a: i32, b: i32, trace: Option<TraceCtx>) -> Vec<u8> {
    let msg = CalcRequest { a, b, trace };
    let payload = msg.encode_to_vec();
    frame(OPC_REQ, &payload)
}

pub fn encode_calc_response(resp: &CalcResponse) -> Vec<u8> {
    let payload = resp.encode_to_vec();
    frame(OPC_RESP, &payload)
}

pub fn decode_calc_request(data: &[u8]) -> Result<CalcRequest, DecodeError> {
    let (payload, _ver, opc) = split_and_check(data)?;
    if opc != OPC_REQ {
        return Err(DecodeError::UnknownOpcode(opc));
    }
    Ok(CalcRequest::decode(payload)?)
}

pub fn decode_calc_response(data: &[u8]) -> Result<CalcResponse, DecodeError> {
    let (payload, _ver, opc) = split_and_check(data)?;
    if opc != OPC_RESP {
        return Err(DecodeError::UnknownOpcode(opc));
    }
    Ok(CalcResponse::decode(payload)?)
}
