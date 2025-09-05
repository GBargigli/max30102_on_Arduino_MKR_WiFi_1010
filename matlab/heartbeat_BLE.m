%% This helper function connects to a BLE device (Arduino MKR WiFi 1010) and receives data from the MAX30102 sensor.
% It is called by the script SNR2 and takes as input the desired acquisition duration in seconds.
% If no input is provided, the default duration is set to 40 seconds.

function heartbeat_BLE(numSec)
    if nargin < 1
        numSec = 40; % Default acquisition duration (in seconds) for Red and IR signal
    end

    % Clear global variables to avoid residual data from previous run
    clear global dataArray dataIndex tempdata tempIndex recFreq;

    % Define UUIDs for BLE service and characteristics
    serviceUUID = '8ee10201-ce06-438a-9e59-549e3a39ba35';
    cmdUUID     = 'bf789fb6-f22d-43b5-bf9e-d5a166a86afa';
    dataUUID    = '33303cf6-3aa4-44ad-8e3c-21084d9b08db'; 
    tempUUID = '44404cf7-4bb5-55be-9f60-32195a8c09ec';
    
    % Specify the BLE device name to connect to
    deviceName = 'AA Pulse Oximeter';
    
    % Global Scope Variables
    global dataArray dataIndex tempdata tempIndex recFreq numSec;
    recFreq=0;

    
    % Preallocate memory for signal and temperature data
    % Signal data: [timestamp, IR, RED]
    % Temperature data: [timestamp, temperature


    % Maximum number of data to be saved
    maxData = numSec*100;
    dataArray = zeros(maxData,3);
    % Index for data storage
    dataIndex = 1;

    tempdata = zeros(numSec, 2);
    % Index for data storage
    tempIndex = 1;
    
    %% Establish BLE connection and configure characteristics
    
    fprintf('Connection to BLE device "%s"...\n', deviceName);
    
    try
        % Disconnect any existing BLE connection to ensure a clean start
        if exist('deviceBLE', 'var') && isvalid(deviceBLE)
            if deviceBLE.Connected
                disconnect(deviceBLE);
                pause(1); % waiting for safety
            end
            clear deviceBLE
        end
    
        % Create a new BLE connection to the specified device
        deviceBLE = ble(deviceName);
    
    catch ME
        error('BLE connection error: %s', ME.message);
    end
    
    % Wait until the device is connected
    while ~deviceBLE.Connected
        pause(0.1);
    end
    fprintf('Device connected!\n');
    
    % Retrieve BLE characteristics for command, signal data, and temperature
    charCmd = characteristic(deviceBLE, serviceUUID, cmdUUID);
    charData = characteristic(deviceBLE, serviceUUID, dataUUID); 
    charTemp = characteristic(deviceBLE, serviceUUID, tempUUID);
    
    % Assign callback functions to handle incoming data packets
    charData.DataAvailableFcn = @(src, evt) notifyCallbackHeartbeat(src, evt);
    charTemp.DataAvailableFcn = @(src, evt) tempCallbackHeartbeat(src, evt);

    % Subscribe to BLE notifications 
    subscribe(charData, "notification");
    subscribe(charTemp, "notification");
    
    fprintf("BLE notifications enabled.\n");
    
    %% Start acquisition
    fprintf("Reading for %d seconds...\n", numSec);
    write(charCmd, uint8(1)); % send command '1' → Arduino starts measurement
    pause(numSec);            % wait for acquisition time 
    % (with pause the execution stops, no background operation that may
    % slow down BLE transmission are performed.

    % Stop acquisition and trigger temperature transmission by sending command '0'
    write(charCmd, uint8(0));  
    fprintf("Acquisition completed.\n"); 

    fprintf("Waiting for asynchronous temperature reception...\n");
    
    pause(30);


    % ====== DATA RECEPTION STATISTICS ======
    % Display number of received signal samples and compute acquisition fr

    fprintf("Total samples received: %d\n", dataIndex-1);  

    t = dataArray(1:dataIndex-1, 1);  % timestamp in microseconds
    t_sec = (t - t(1)) / 1e6;  % Convert timestamps to seconds and normalize to start from zero
    recFreq = (length(t)) / (t_sec(end));  % f = total samples / duration
    fprintf("Average frequency: %.2f Hz\n", recFreq);

    fprintf("Total temperatures received: %d\n", tempIndex-1);

    %% DEBUG
    % fprintf('\nTemperature and timestamp data:\n');
    % fprintf('---------------------------\n');
    % fprintf('Index | Timestamp (ms) | Temperature (°C)\n');
    % fprintf('---------------------------\n');
    % 
    % for idx = 1:(tempIndex-1)
    %     fprintf('%6d | %14d | %14.2f\n', idx, tempdata(idx, 1), tempdata(idx, 2));
    % end
    % 
    % fprintf('---------------------------\n');
end

function notifyCallbackHeartbeat(src, ~)
    % Callback triggered when a signal data packet is received (100 bytes = 10 samples)
    % receives 100-byte packet (10 samples of 10 bytes each)
    global dataArray dataIndex;

    try
        data = read(src, 'latest'); % try to read latest notified package
    catch ME
        warning(ME.identifier, 'No data available or device disconnected: %s', ME.message);
        return; % returns without doing anything
    end
    
    % % DEBUG: 
    % % prints the length for the first 10 packages 
    % global dataIndex;
    % if dataIndex <= 10
    %     fprintf("Bytes received: %d\n", length(data));
    % end

    nSamples = 10;

    %% Decode IR, RED, and timestamp values from each sample
    for i = 0:(nSamples-1)
        idx = i*10;  % sample index in data vector
        
        % IR
        irVal = bitshift(uint32(data(idx+1)), 16) + ...
        bitshift(uint32(data(idx+2)), 8) + ...
        uint32(data(idx+3));

        % RED
        redVal = bitshift(uint32(data(idx+4)), 16) + ...
        bitshift(uint32(data(idx+5)), 8) + ...
        uint32(data(idx+6));
        
        % TIME (4 byte little endian)
        % byte 7 + byte 8*2^8 + byte 9*2^16 + byte 10*2^24
        timeVal = double(data(idx+7)) + double(data(idx+8)) * 256 + double(data(idx+9)) * 65536 + double(data(idx+10)) * 16777216;

        %% Store decoded values in the preallocated array if within boun
        if dataIndex <= size(dataArray,1)
            dataArray(dataIndex,:) = [timeVal, irVal, redVal];
            dataIndex = dataIndex + 1;
        end
    end
end



function tempCallbackHeartbeat(src, ~)
% Callback triggered when a temperature data packet is received

    fprintf('tempCallbackHeartbeat called\n');

    global tempdata tempIndex;

    try
        tempBytes = read(src, 'latest'); % receive 80 bytes 
    catch ME
        warning(ME.identifier,'No temperature data available or device disconnected: %s', ME.message);
        return;
    end

    %% DEBUG: print length in bytes and number of T° packets received
    %nBytes = length(tempBytes);
    %nEntries = nBytes / 8;
    %fprintf("Temperature packet received: %d byte (%d measures)\n", nBytes, nEntries);
    %fprintf('tempIndex before writing: %d\n', tempIndex); 
    % if mod(nBytes,8) ~= 0
    % warning("Temperature packet not a multiple of 8 bytes!");
    % end


    %% Read and decode each temperature entry (8 bytes: 4 for temperature, 4 for timestamp)
    for i = 0:(nEntries - 1) % Cycle on every measure in the package
        % I calculate the offset in the tempBytes vector where the i-th measurement starts
        offset = i * 8;

        % Extract temperature (4 byte float)
        tempFloatBytes = tempBytes(offset + (1:4));
        tempVal = typecast(uint8(tempFloatBytes), 'single');

        % Extract timestamp (4 byte little-endian uint32)
        timeBytes = tempBytes(offset + (5:8));
        timestamp = typecast(uint8(timeBytes), 'uint32');        
        timestamp = double(timestamp); % Convert timestamp to double for consistency

        if tempIndex <= size(tempdata, 1) 
            %fprintf("Writing tempIndex=%d: timestamp=%d, temp=%.2f\n", tempIndex, timestamp, tempVal);
            tempdata(tempIndex, :) = [timestamp, tempVal];
            tempIndex = tempIndex + 1;
        end
    end

    pause(0.001);

 end
