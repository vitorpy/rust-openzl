// Copyright (c) Meta Platforms, Inc. and affiliates.

import {EdgeLabelRenderer, type EdgeProps} from '@xyflow/react';

const getOffsetPath = (sourceX: number, sourceY: number, targetX: number, targetY: number, offset: any) => {
  const centerX = (sourceX + targetX) / 2 + offset;
  const centerY = (sourceY + targetY) / 2;

  return {
    edgePath: `M ${sourceX} ${sourceY} Q ${centerX} ${centerY} ${targetX} ${targetY}`,
    labelX: centerX,
    labelY: centerY,
  };
};

export default function CustomEdge({id, sourceX, sourceY, targetX, targetY, label, data}: EdgeProps) {
  const vals = getOffsetPath(sourceX, sourceY, targetX, targetY, data!.offset);

  return (
    <>
      <path id={id} className="react-flow__edge-path" d={vals.edgePath} stroke="black" strokeWidth="5" />
      <EdgeLabelRenderer>
        <div
          style={{
            transform: `translate(-50%, -50%) translate(${vals.labelX}px,${vals.labelY}px)`,
            borderStyle: 'solid',
            borderColor: 'black',
            borderWidth: '1px',
            padding: '5px',
          }}
          className="nodrag nopan edge-label">
          {label}
        </div>
      </EdgeLabelRenderer>
    </>
  );
}

export const edgeTypes = {
  custom: CustomEdge,
};
