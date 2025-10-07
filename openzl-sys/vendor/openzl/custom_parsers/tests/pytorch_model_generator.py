# Copyright (c) Meta Platforms, Inc. and affiliates.

import hashlib
import os
import random
import sys
import tempfile
import zipfile


def write_data_file(zf, filename, small):
    path = ""

    if random.random() < 0.5:
        path += "subdir/"

    if random.random() < 0.5:
        path += "data/"
    else:
        path += "xl_model_weights/"

    if random.random() < 0.5:
        path += "suffix/"

    path += filename
    data = random.randbytes(random.randint(0, 100 if small else 10000))
    zf.writestr(path, data)


def write_other_file(zf, filename, small):
    path = ""
    if random.random() < 0.5:
        path += "code/"
    path += filename
    data = random.randbytes(random.randint(0, 100 if small else 10000))
    zf.writestr(path, data)


def generate_zipfile(small):
    with tempfile.NamedTemporaryFile() as f:
        with zipfile.ZipFile(f.name, "w") as zf:
            num_files = random.randint(0, 20)
            for i in range(num_files):
                if random.random() < 0.5:
                    write_data_file(zf, str(i), small)
                else:
                    write_other_file(zf, str(i), small)

        with open(f.name, "rb") as f:
            return f.read()


def generate_corpus(small):
    for _ in range(100):
        yield generate_zipfile(small)


def main(test_suite, test_case, out_dir):
    if test_suite not in ("PytorchModelParserTest", "ZipLexerTest"):
        raise ValueError(f"Unknown test suite: {test_suite}")
    small = test_suite == "ZipLexerTest"

    corpus = generate_corpus(small)

    os.makedirs(out_dir, exist_ok=True)
    for blob in corpus:
        sha = hashlib.sha256(blob).hexdigest()
        path = os.path.join(out_dir, sha)
        with open(path, "wb") as f:
            f.write(blob)


if __name__ == "__main__":
    main(*sys.argv[1:])
