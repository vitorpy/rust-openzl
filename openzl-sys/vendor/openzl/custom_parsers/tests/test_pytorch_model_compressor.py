#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.


import os
import shutil
import subprocess
import sys
import tempfile


def main(generator, compressor):
    try:
        tmpdir = tempfile.mkdtemp()
        subprocess.check_call(
            [generator, "PytorchModelParserTest", "TestRoundTrip", tmpdir]
        )
        for file in os.listdir(tmpdir):
            path = os.path.join(tmpdir, file)
            subprocess.check_call([compressor, path])
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    main(*sys.argv[1:])
