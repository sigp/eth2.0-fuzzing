use ssz::{Encode, Decode};
use ssz_derive::{Encode, Decode};
use types::{BeaconState, BeaconBlock, EthSpec, FoundationEthSpec};
use std::{slice, ptr};
use libc::{uint8_t, size_t};
use state_processing::{BlockProcessingError, per_block_processing};

#[derive(Decode, Encode)]
struct BlockTestCase<T: EthSpec> {
    pub pre: BeaconState<T>,
    pub block: BeaconBlock,
}

impl<T: EthSpec> BlockTestCase<T> {
    /// Run `per_block_processing` and return a `BeaconState` on success, or a
    /// `BlockProcessingError` on failure.
    fn process_block(mut self) -> Result<BeaconState<T>, BlockProcessingError> {
        let spec = T::spec();

        per_block_processing(&mut self.pre, &self.block, &spec)?;

        Ok(self.pre)
    }
}

/// Accepts an SSZ-encoded `BlockTestCase` and returns an SSZ-encoded post-state on success,
/// or nothing on failure.
fn fuzz<T: EthSpec>(ssz_bytes: &[u8]) -> Result<Vec<u8>, ()> {
    let test_case = match BlockTestCase::from_ssz_bytes(&ssz_bytes) {
        Ok(test_case) => test_case,
        _ => return Err(())
    };

    let post_state: BeaconState<T> = match test_case.process_block() {
        Ok(state) => state,
        _ => return Err(())
    };

    Ok(post_state.as_ssz_bytes())
}

#[no_mangle]
pub fn block_c(
    input_ptr: *mut uint8_t,
    input_size: size_t,
    output_ptr: *mut uint8_t,
    output_size: *mut size_t) -> bool {

    let input_bytes: &[u8] = unsafe {
        slice::from_raw_parts(input_ptr, input_size as usize)
    };

    // Note: `FoundationEthSpec` contains the "constants" in the official spec.
    if let Ok(output_bytes) = fuzz::<FoundationEthSpec>(input_bytes) {
        unsafe {
            if output_bytes.len() > *output_size {
                return false;
            }
            ptr::copy_nonoverlapping(output_bytes.as_ptr(), output_ptr, output_bytes.len());
            *output_size = output_bytes.len();
        }

        return true;
    } else {
        return false;
    }
}
