// Copyright (c) Meta Platforms, Inc. and affiliates.

import {Streamdump} from '../models/Streamdump';

// Useful for functions that wait for streamdump data to do something
export type NullableStreamdump = {
  data: Streamdump | null;
};
