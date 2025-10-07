// Copyright (c) Meta Platforms, Inc. and affiliates.

import type {SerializedStream} from './SerializedStream';
import type {SerializedCodec} from './SerializedCodec';
import type {SerializedGraph} from './SerializedGraph';

export interface SerializedStreamdump {
  streams: SerializedStream[];
  codecs: SerializedCodec[];
  graphs: SerializedGraph[];
}
