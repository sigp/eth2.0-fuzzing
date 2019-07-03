#define GO_FUZZ_PREFIX block_
#include <lib/differential.h>
#include <lib/python.h>
#include <lib/go.h>
#include <lib/rust.h>
#include <lib/ssz-preprocess.h>
#include <cstring>

#ifndef PYTHON_HARNESS_PATH
#error PYTHON_HARNESS_PATH undefined
#endif

#ifndef PYTHON_LIB_PATH
#error PYTHON_LIB_PATH undefined
#endif

extern "C" bool block_c(uint8_t* input_ptr, size_t input_size, uint8_t* output_ptr, size_t* output_size);

namespace fuzzing {
    class Lighthouse : public Rust {
        std::optional<std::vector<uint8_t>> run(const std::vector<uint8_t>& _data) override {
            /* Copy because block_c wants a non-const pointer */
            std::vector<uint8_t> data(_data.data(), _data.data() + _data.size());

            size_t output_size = data.size() * 4;
            std::vector<uint8_t> ret(output_size);

            if ( block_c(data.data(), data.size(), ret.data(), &output_size) == false ) {
                return std::nullopt;
            }

            ret.resize(output_size);

            return ret;
        }
    };
} /* namespace fuzzing */

std::shared_ptr<fuzzing::Python> PySpec = nullptr;
std::shared_ptr<fuzzing::Go> zrnt = nullptr;
std::shared_ptr<fuzzing::Lighthouse> lighthouse = nullptr;

std::unique_ptr<fuzzing::Differential> differential = nullptr;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    differential = std::make_unique<fuzzing::Differential>();

    differential->AddModule(
        PySpec = std::make_shared<fuzzing::Python>((*argv)[0], PYTHON_HARNESS_PATH, PYTHON_LIB_PATH)
    );
    differential->AddModule(
        zrnt = std::make_shared<fuzzing::Go>()
    );
    differential->AddModule(
        lighthouse = std::make_shared<fuzzing::Lighthouse>()
    );

    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t *data, size_t size) {
    auto v = fuzzing::SSZPreprocess(data, size);
    if ( v.empty() ) {
        return 0;
    }

    differential->Run(v);

    return 0;
}
