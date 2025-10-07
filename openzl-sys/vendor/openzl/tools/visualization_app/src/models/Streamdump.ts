// Copyright (c) Meta Platforms, Inc. and affiliates.

import type {SerializedStreamdump} from '../interfaces/SerializedStreamdump';
import {Codec} from './Codec';
import {Stream} from './Stream';
import {Graph} from './Graph';

export class Streamdump {
  readonly streams: Stream[];
  readonly codecs: Codec[];
  readonly graphs: Graph[];

  constructor(streams: Stream[], codecs: Codec[], graphs: Graph[]) {
    this.streams = streams;
    this.codecs = codecs;
    this.graphs = graphs;
  }

  static fromObject(obj: SerializedStreamdump): Streamdump {
    return new Streamdump(
      obj.streams.map((stream, streamId) => Stream.fromObject(stream, streamId)),
      obj.codecs.map((codec, codecNum) => Codec.fromObject(codec, codecNum)),
      obj.graphs.map((graph, graphNum) => Graph.fromObject(graph, graphNum)),
    );
  }
}
