// Copyright (c) Meta Platforms, Inc. and affiliates.

import type {Stream} from '../../models/Stream';
import type {InternalNode} from './InternalNode';

export class InternalEdge {
  id: string;
  stream: Stream;
  source: InternalNode;
  target: InternalNode;
  sourceHandle: string;
  targetHandle: string;
  label: string;
  type: string;
  style: Record<string, string | number>;
  hidden: boolean = false;
  inLargestCompressionPath: boolean = false;

  constructor(
    id: string,
    stream: Stream,
    source: InternalNode,
    target: InternalNode,
    sourceHandle: string,
    targetHandle: string,
    label: string,
    type: string,
    style: Record<string, string | number> = {},
  ) {
    this.id = id;
    this.stream = stream;
    this.source = source;
    this.target = target;
    this.sourceHandle = sourceHandle;
    this.targetHandle = targetHandle;
    this.label = label;
    this.type = type;
    this.style = style;
  }
}
