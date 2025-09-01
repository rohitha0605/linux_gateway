use crc32fast::Hasher;
use thiserror::Error;

/// On-wire frame:
/// [0xAA,0x55] [ver: u8] [typ: u8] [len: u16 LE] [payload .. len ..] [crc32 LE]
/// CRC32 is over: ver || typ || len_le || payload
#[derive(Debug, Error, PartialEq, Eq)]
pub enum WireError {
    #[error("truncated frame")]
    Short,
    #[error("length mismatch")]
    BadLen,
    #[error("bad preamble")]
    BadHeader,
    #[error("crc mismatch")]
    BadCrc,
}

const PREAMBLE: [u8; 2] = [0xAA, 0x55];

pub fn build_frame(ver: u8, typ: u8, payload: &[u8]) -> Vec<u8> {
    let len = payload.len() as u16;
    let len_le = len.to_le_bytes();

    let mut out = Vec::with_capacity(2 + 1 + 1 + 2 + payload.len() + 4);
    out.extend_from_slice(&PREAMBLE);
    out.push(ver);
    out.push(typ);
    out.extend_from_slice(&len_le);
    out.extend_from_slice(payload);

    let mut h = Hasher::new();
    h.update(&[ver]);
    h.update(&[typ]);
    h.update(&len_le);
    h.update(payload);
    let crc = h.finalize().to_le_bytes();
    out.extend_from_slice(&crc);
    out
}

pub fn parse_and_verify(frame: &[u8]) -> Result<(Vec<u8>, u8, u8), WireError> {
    // Minimum frame size with empty payload
    if frame.len() < 2 + 1 + 1 + 2 + 4 {
        return Err(WireError::Short);
    }
    if frame[0..2] != PREAMBLE {
        return Err(WireError::BadHeader);
    }

    let ver = frame[2];
    let typ = frame[3];
    let len = u16::from_le_bytes([frame[4], frame[5]]) as usize;

    let expected = 2 + 1 + 1 + 2 + len + 4;
    if frame.len() < expected {
        return Err(WireError::Short);
    }
    if frame.len() != expected {
        return Err(WireError::BadLen);
    }

    let payload = &frame[6..6 + len];
    let crc_got = u32::from_le_bytes(frame[6 + len..6 + len + 4].try_into().unwrap());

    let mut h = Hasher::new();
    h.update(&[ver]);
    h.update(&[typ]);
    h.update(&(len as u16).to_le_bytes());
    h.update(payload);
    let crc_exp = h.finalize();

    if crc_exp != crc_got {
        return Err(WireError::BadCrc);
    }

    Ok((payload.to_vec(), ver, typ))
}
