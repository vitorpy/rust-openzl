// Copyright (c) Meta Platforms, Inc. and affiliates.

import {decode} from 'cbor2';
import type {SerializedStreamdump} from '../interfaces/SerializedStreamdump';
import {Streamdump} from '../models/Streamdump';

// Validate that the object loaded in is the contents of streamdump
function isDecodedStreamdump(obj: unknown): obj is SerializedStreamdump {
  return (
    typeof obj === 'object' &&
    obj !== null &&
    'streams' in obj &&
    Array.isArray(obj.streams) &&
    'codecs' in obj &&
    Array.isArray(obj.codecs) &&
    'graphs' in obj &&
    Array.isArray(obj.graphs)
  );
}

export async function extractStreamdumpFromCborFile(file: File): Promise<Streamdump> {
  let decodedCborData: SerializedStreamdump;
  // load the data
  try {
    const buffer = await file.arrayBuffer();
    decodedCborData = decode(new Uint8Array(buffer));
  } catch (error) {
    console.error('Error decoding CBOR file', error);
    throw error;
  }
  // Validate that this object is of proper structure
  if (!isDecodedStreamdump(decodedCborData)) {
    throw new Error("Decoded data does not match the expected structure of streamdump's contents.");
  }

  return Streamdump.fromObject(decodedCborData);
}
