% THIS HELPER FUNCTION RECEIVES AND STORES MAX30102-BLUETOOTH-TRANSMITTED-DATA
% it is called by the script SNR2

function heartbeat_BLE(numSec)
    if nargin < 1
        numSec = 40; % default 40 secondi
    end

    % Pulisci le variabili globali da eventuali script precedenti
    clear global dataArray dataIndex tempdata tempIndex fricezione;

    % UUID del servizio e delle caratteristiche (usa i tuoi UUID reali qui)
    serviceUUID = '8ee10201-ce06-438a-9e59-549e3a39ba35';
    cmdUUID     = 'bf789fb6-f22d-43b5-bf9e-d5a166a86afa';
    dataUUID    = '33303cf6-3aa4-44ad-8e3c-21084d9b08db'; % caratteristica singola dati (100 byte)
    tempUUID = '44404cf7-4bb5-55be-9f60-32195a8c09ec';
    
    % Nome del dispositivo BLE
    deviceName = 'AA Pulse Oximeter';
    
    % variabili Global Scope
    global dataArray dataIndex tempdata tempIndex fricezione numSec;
    fricezione=0;

    % PREALLOCAZIONI

    % Pre-allocazione array per i dati: colonne = [millis, Temp]
    % Numero massimo di dati da salvare
    maxData = numSec*100;
    dataArray = zeros(maxData,3);
    % Indice per salvataggio dati
    dataIndex = 1;

    % Pre-allocazione per dati temperatura: colonne = [timestamp, temperatura]
    tempdata = zeros(numSec, 2);
    tempIndex = 1;
    
    %% Connessione BLE e setup caratteristiche
    
    fprintf('Connessione al dispositivo BLE "%s"...\n', deviceName);
    
    try
        % Se esiste una connessione attiva, disconnettila
        if exist('deviceBLE', 'var') && isvalid(deviceBLE)
            if deviceBLE.Connected
                disconnect(deviceBLE);
                pause(1); % attesa per sicurezza
            end
            clear deviceBLE
        end
    
        % Crea nuova connessione
        deviceBLE = ble(deviceName);
    
    catch ME
        error('Errore di connessione BLE: %s', ME.message);
    end
    
    % Attendi che il dispositivo sia connesso
    while ~deviceBLE.Connected
        pause(0.1);
    end
    fprintf('Dispositivo connesso!\n');
    
    % Ottengo le caratteristiche
    charCmd = characteristic(deviceBLE, serviceUUID, cmdUUID);
    charData = characteristic(deviceBLE, serviceUUID, dataUUID); 
    charTemp = characteristic(deviceBLE, serviceUUID, tempUUID);
    
    % Imposta callback per dati (viene chiamata ogni pacchetto di 100 byte)
    charData.DataAvailableFcn = @(src, evt) notifyCallbackHeartbeat(src, evt);
    charTemp.DataAvailableFcn = @(src, evt) tempCallbackHeartbeat(src, evt);

    % Abilita notifiche BLE
    subscribe(charData, "notification");
    subscribe(charTemp, "notification");
    
    fprintf("Notifiche BLE abilitate.\n");
    
    %% Inizio acquisizione
    fprintf("Lettura per %d secondi...\n", numSec);
    write(charCmd, uint8(1)); % invia comando '1' → Arduino inizia misurazione
    pause(numSec);            % aspetta il tempo di acquisizione
    write(charCmd, uint8(0));  % invia comando '0' → Arduino ferma misurazione
    fprintf("Acquisizione terminata.\n"); 

    fprintf("Attendo ricezione asincrona temperatura (via callback)...\n");
    
    pause(30);

    % ========== STATISTICHE DI RICEZIONE DATI ================
    % n° dati ricevuti e frequenza ricezione dati
    fprintf("Totale campioni ricevuti: %d\n", dataIndex-1);  

    t = dataArray(1:dataIndex-1, 1);  % timestamp in micros
    t_sec = (t - t(1)) / 1e6;  % converti in secondi e rendi relativo (t(0) = 0)
    fricezione = (length(t)) / (t_sec(end));  % totale campioni / durata
    fprintf("Frequenza media: %.2f Hz\n", fricezione);

    fprintf("Totale temperature ricevute: %d\n", tempIndex-1);

    % fprintf('\nDati temperatura e timestamp:\n');
    % fprintf('---------------------------\n');
    % fprintf('Indice | Timestamp (ms) | Temperatura (°C)\n');
    % fprintf('---------------------------\n');
    % 
    % for idx = 1:(tempIndex-1)
    %     fprintf('%6d | %14d | %14.2f\n', idx, tempdata(idx, 1), tempdata(idx, 2));
    % end
    % 
    % fprintf('---------------------------\n');
end

function notifyCallbackHeartbeat(src, ~)
    % Callback per la caratteristica dati: 
    % riceve pacchetto da 100 byte (10 campioni da 10 byte ciascuno)
    global dataArray dataIndex;

    try
        data = read(src, 'latest'); % leggi ultimo pacchetto notificato
    catch ME
        warning(ME.identifier, 'Nessun dato disponibile o dispositivo disconnesso: %s', ME.message);
        return; % esci senza fare nulla
    end
    
    % % DEBUG: stampa la lunghezza solo per i primi 10 pacchetti
    % global dataIndex;
    % if dataIndex <= 10
    %     fprintf("Bytes ricevuti: %d\n", length(data));
    % end

    nSamples = 10;
    
    for i = 0:(nSamples-1)
        idx = i*10;  % indice base del campione nel vettore dati
        
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

        % Salva i dati solo se non si è raggiunto il limite massimo di campioni
        if dataIndex <= size(dataArray,1)
            dataArray(dataIndex,:) = [timeVal, irVal, redVal];
            dataIndex = dataIndex + 1;
        end
    end
end



function tempCallbackHeartbeat(src, ~)
  
    fprintf('tempCallbackHeartbeat chiamato\n');

    global tempdata tempIndex;

    try
        tempBytes = read(src, 'latest'); % ricevi 80 byte 
    catch ME
        warning(ME.identifier,'Nessun dato temperatura disponibile o dispositivo disconnesso: %s', ME.message);
        return;
    end

    %DEBUG: print lunghezza in byte e numero dei pacchetti di T° ricevuti
    nBytes = length(tempBytes);
    nEntries = nBytes / 8;
    fprintf("Pacchetto temperatura ricevuto: %d byte (%d misure)\n", nBytes, nEntries);
    fprintf('tempIndex prima di scrivere: %d\n', tempIndex); 
    % if mod(nBytes,8) ~= 0
    % warning("Pacchetto temperatura di lunghezza non multipla di 8 bytes!");
    % end

    for i = 0:(nEntries - 1) % Ciclo su ogni misura contenuta nel pacchetto
        % Calcolo l'offset nel vettore tempBytes dove inizia la i-esima misura
        offset = i * 8;

        % Estrai temperatura (4 byte float)
        tempFloatBytes = tempBytes(offset + (1:4));
        tempVal = typecast(uint8(tempFloatBytes), 'single');

        % Estrai timestamp (4 byte little-endian uint32)
        timeBytes = tempBytes(offset + (5:8));
        timestamp = typecast(uint8(timeBytes), 'uint32');        
        timestamp = double(timestamp); % Converti in double

        if tempIndex <= size(tempdata, 1) 
            %fprintf("Scrivo tempIndex=%d: timestamp=%d, temp=%.2f\n", tempIndex, timestamp, tempVal);
            tempdata(tempIndex, :) = [timestamp, tempVal];
            tempIndex = tempIndex + 1;
        end
    end

    pause(0.001);

 end
