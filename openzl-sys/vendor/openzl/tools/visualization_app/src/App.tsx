// Copyright (c) Meta Platforms, Inc. and affiliates.

import {useState, useRef} from 'react';
import './App.css';
import {extractStreamdumpFromCborFile} from './utils/decodeCbor.ts';
import {StreamdumpGraph} from './components/StreamdumpGraph.tsx';
import {Streamdump} from './models/Streamdump.ts';
import Toolbar from './components/Toolbar.tsx';
import {Box} from '@chakra-ui/react';

export default function App() {
  const [cborData, setCborData] = useState<Streamdump | null>(null);
  // Values required to update input the visualization when uploading a file
  const [fileInputKey, setFileInputKey] = useState<number>(0);
  const fileInputRef = useRef<HTMLInputElement>(null);

  // Invoke to decode and display a new file uploaded
  const handleFileChange = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (!file) {
      console.log('No file uploaded yet!');
      return;
    }
    try {
      const data = await extractStreamdumpFromCborFile(file);
      setCborData(data);

      // Force a React re-render to update the input streamdump to decode and display
      setFileInputKey((prev) => prev + 1);
    } catch (err) {
      console.error('Failed to decode CBOR file:', err);
    }
  };

  // Used to input a new serialized CBOR file to display
  const handleFileButtonClick = () => {
    // Invoke receiving new input
    fileInputRef.current?.click();
  };

  return (
    <div className="wrapper">
      <Toolbar onUploadCborFile={handleFileButtonClick} />

      <div className="content">
        {/* Used to input a new file. This <input /> is needed, as a button in the toolbar invoke this input, to open file explorer and load new data once something is uploaded */}
        <input
          key={fileInputKey}
          ref={fileInputRef}
          type="file"
          onChange={handleFileChange}
          style={{display: 'none'}}
        />

        <Box h="100%" w="100%" paddingLeft={'5%'} paddingRight={'5%'} paddingTop={4} paddingBottom={4}>
          <StreamdumpGraph data={cborData} />
        </Box>
      </div>
    </div>
  );
}
