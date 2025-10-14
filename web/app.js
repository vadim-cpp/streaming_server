class AsciiStreamer 
{
    constructor() 
    {
        this.output = document.getElementById('asciiOutput');
        this.ws = null;
        this.isStreaming = false;
        this.apiInfo = document.getElementById('apiInfo');
        this.copyApiBtn = document.getElementById('copyApiBtn');
        this.showApiBtn = document.getElementById('showApiBtn');
        this.api_key = null;
        
        this.cameraSelect = document.getElementById('camera');
        this.microphoneSelect = document.getElementById('microphone');
        
        this.recordBtn = document.getElementById('recordBtn');
        this.isRecording = false;
        
        this.subtitleElement = null;
        this.subtitleTimeout = null;
        
        this.init();
    }

    init() 
    {
        this.loadCameras();
        this.loadMicrophones();
        this.setupEventListeners();
        this.createSubtitleElement();
    }

    setupEventListeners() 
    {
        // Обработчики кнопок API
        this.showApiBtn.addEventListener('click', () => this.showApiInfo());
        this.copyApiBtn.addEventListener('click', () => this.copyApiInfo());
        
        // Основная кнопка запуска стрима
        document.getElementById('startBtn').addEventListener('click', () => {
            if (this.isStreaming) this.stop();
            else this.start();
        });

        // Кнопка записи
        this.recordBtn.addEventListener('click', () => this.toggleRecording());
        
        // Кнопка тестирования туннеля
        document.getElementById('testTunnelBtn').addEventListener('click', () => this.testTunnel());
        
        // Кнопка просмотра записей
        document.getElementById('recordingsBtn').addEventListener('click', () => {
            window.location.href = 'recordings.html';
        });
    }

    createSubtitleElement() 
    {
        this.subtitleElement = document.createElement('div');
        this.subtitleElement.id = 'subtitles';
        this.subtitleElement.style.cssText = `
            position: fixed;
            bottom: 20px;
            left: 50%;
            transform: translateX(-50%);
            background: rgba(0, 0, 0, 0.8);
            color: white;
            padding: 10px 20px;
            border-radius: 5px;
            max-width: 80%;
            text-align: center;
            font-family: 'Courier New', monospace;
            font-size: 18px;
            z-index: 1000;
            display: none;
            white-space: pre-wrap;
            word-wrap: break-word;
        `;
        document.body.appendChild(this.subtitleElement);
    }

    showSubtitles(text) 
    {
        if (!this.subtitleElement) return;
        
        this.subtitleElement.textContent = text;
        this.subtitleElement.style.display = 'block';
        
        // Автоматически скрываем через 5 секунд
        clearTimeout(this.subtitleTimeout);
        this.subtitleTimeout = setTimeout(() => {
            this.hideSubtitles();
        }, 5000);
    }

    hideSubtitles() 
    {
        if (this.subtitleElement) 
        {
            this.subtitleElement.style.display = 'none';
        }
    }

    async toggleRecording() 
    {
        if (this.isRecording) 
        {
            this.stopRecording();
        } 
        else 
        {
            this.startRecording();
        }
    }
    
    async startRecording() 
    {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN)
        {
            alert('Please start the stream first');
            return;
        }
        
        this.ws.send(JSON.stringify({
            type: 'record_start'
        }));
        
        this.recordBtn.textContent = 'Stop Recording';
        this.recordBtn.style.background = '#ff4444';
        this.isRecording = true;
    }
    
    async stopRecording() 
    {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) 
        {
            this.ws.send(JSON.stringify({
                type: 'record_stop'
            }));
        }
        
        this.recordBtn.textContent = 'Start Recording';
        this.recordBtn.style.background = '';
        this.isRecording = false;
    }

    async showApiInfo() 
    {
        try 
        {
            const response = await fetch('/api');
            const data = await response.json();
            
            this.apiInfo.textContent = `Endpoint: ${data.endpoint}\nAPI Key: ${data.api_key}`;
            this.apiInfo.style.display = 'block';
            this.copyApiBtn.style.display = 'block';
        } 
        catch (error) 
        {
            console.error('Failed to get API info:', error);
            this.apiInfo.textContent = 'Error: Could not fetch API info';
            this.apiInfo.style.display = 'block';
        }
    }

    copyApiInfo() 
    {
        navigator.clipboard.writeText(this.apiInfo.textContent)
            .then(() => alert('API info copied to clipboard!'))
            .catch(err => console.error('Copy failed:', err));
    }

    async loadCameras() 
    {
        try 
        {
            const response = await fetch('/cameras');
            const cameras = await response.json();
            this.populateCameraSelect(cameras);
        } 
        catch (error)
        {
            console.error('Failed to load cameras:', error);
        }
    }

    populateCameraSelect(cameras) 
    {
        this.cameraSelect.innerHTML = '';
        cameras.forEach(camera => {
            const option = document.createElement('option');
            option.value = camera.index;
            option.textContent = camera.name;
            this.cameraSelect.appendChild(option);
        });
    }

    async loadMicrophones() 
    {
        try 
        {
            const response = await fetch('/microphones');
            const microphones = await response.json();
            this.populateMicrophoneSelect(microphones);
        } 
        catch (error) 
        {
            console.error('Failed to load microphones:', error);
        }
    }

    populateMicrophoneSelect(microphones) 
    {
        this.microphoneSelect.innerHTML = '<option value="">No microphone</option>';
        microphones.forEach(mic => {
            const option = document.createElement('option');
            option.value = mic.index;
            option.textContent = mic.name;
            this.microphoneSelect.appendChild(option);
        });
    }

    async start() 
    {
        if (this.isStreaming) return;

        try 
        {
            this.updateUI(true);
            this.output.textContent = "Starting stream...";
            this.isStreaming = true;
            
            if (!this.api_key) 
            {
                this.api_key = await this.getApiKey();
            }
            
            this.ws = new WebSocket(`wss://${window.location.host}/stream`);
            
            this.ws.onopen = () => {
                this.ws.send(JSON.stringify({
                    type: 'auth',
                    api_key: this.api_key,
                    role: 'controller'
                }));

                // Запускаем аудио захват на сервере (если выбран микрофон)
                const microphoneIndex = this.microphoneSelect.value;
                if (microphoneIndex) {
                    this.ws.send(JSON.stringify({
                        type: 'start_audio_capture',
                        microphone_index: parseInt(microphoneIndex)
                    }));
                }
            };
            
            this.ws.onmessage = (event) => {
                try 
                {
                    const data = JSON.parse(event.data);
                    
                    if (data.frame) 
                    {
                        // Это фрейм с субтитрами
                        this.output.textContent = data.frame;
                        
                        if (data.subtitles) 
                        {
                            this.showSubtitles(data.subtitles);
                        }
                    } 
                    else 
                    {
                        // Обрабатываем текстовые команды
                        this.handleControlMessage(event.data);
                    }
                } 
                catch (e) 
                {
                    // Если не JSON, обрабатываем как обычное текстовое сообщение
                    this.handleControlMessage(event.data);
                }
            };

            this.ws.onclose = () => {
                if (this.isStreaming) {
                    this.stop();
                }
            };

            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.stop();
            };

        } catch (error) {
            console.error('Error starting stream:', error);
            alert('Failed to start stream: ' + error.message);
            this.stop();
        }
    }

    handleControlMessage(message) 
    {
        const cleanedMessage = message.replace(/\u0000/g, '').trim();

        switch (cleanedMessage) 
        {
            case "AUTH_CONTROLLER_SUCCESS":
                const cameraIndex = document.getElementById('camera').value;
                const resolution = document.getElementById('resolution').value;
                const fps = 10;
                
                this.ws.send(JSON.stringify({
                    type: 'config',
                    camera_index: parseInt(cameraIndex),
                    resolution: resolution,
                    fps: fps
                }));
                break;
                
            case "CONFIG_APPLIED":
                this.output.textContent = "Stream started successfully";
                break;
                
            case "STREAM_STOPPED":
                this.updateUI(false);
                break;
                
            case "RECORDING_STARTED":
                alert('Recording started successfully');
                break;
                
            case "RECORDING_STOPPED":
                alert('Recording stopped successfully');
                break;
                
            case "RECORDING_ERROR":
                alert('Recording error occurred');
                this.recordBtn.textContent = 'Start Recording';
                this.recordBtn.style.background = '';
                this.isRecording = false;
                break;
                
            case "AUDIO_CAPTURE_STARTED":
                console.log('Audio capture started on server');
                break;
                
            case "AUDIO_CAPTURE_STOPPED":
                console.log('Audio capture stopped on server');
                break;
                
            default:
                // Если это не команда, отображаем как ASCII кадр
                this.output.textContent = cleanedMessage;
                break;
        }
    }

    async stop() 
    {
        // Останавливаем аудио захват на сервере
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify({
                type: 'stop_audio_capture'
            }));
            
            this.ws.send(JSON.stringify({ type: 'stop' }));
            
            // Ждем подтверждения от сервера перед закрытием
            setTimeout(() => {
                if (this.ws) {
                    this.ws.close();
                    this.ws = null;
                }
                this.updateUI(false);
            }, 300);
        } 
        else 
        {
            this.updateUI(false);
        }
        
        this.hideSubtitles();
    }

    updateUI(isStreaming) 
    {
        const startBtn = document.getElementById('startBtn');
        startBtn.textContent = isStreaming ? 'Stop Stream' : 'Start Stream';
        startBtn.style.background = isStreaming ? '#ff4444' : '';
        
        this.isStreaming = isStreaming;
        
        if (!isStreaming) 
        {
            this.output.textContent = "Stream stopped";
            this.hideSubtitles();
            
            // Сбрасываем состояние записи
            if (this.isRecording) 
            {
                this.recordBtn.textContent = 'Start Recording';
                this.recordBtn.style.background = '';
                this.isRecording = false;
            }
        }
    }

    async getApiKey() 
    {
        try 
        {
            const response = await fetch('/api');
            const data = await response.json();
            return data.api_key;
        } 
        catch (error) 
        {
            console.error('Failed to get API key:', error);
            return '';
        }
    }

    async testTunnel() 
    {
        try 
        {
            const response = await fetch('/test_tunnel');
            const data = await response.json();
            
            if (data.success) 
                {
                alert('Tunnel test passed! Your server is accessible from the internet.');
            } 
            else 
            {
                alert('Tunnel test failed: ' + data.message);
            }
        } 
        catch (error) 
        {
            alert('Tunnel test failed: ' + error.message);
        }
    }
}

// Глобальные функции для загрузки страницы
window.addEventListener('load', () => {
    new AsciiStreamer();
});

// Проверка статуса туннеля при загрузке страницы
document.addEventListener('DOMContentLoaded', async () => {
    await checkTunnelStatus();
});

async function checkTunnelStatus() 
{
    try 
    {
        const response = await fetch('/tunnel_status');
        const data = await response.json();
        
        const statusElement = document.getElementById('tunnelStatusText');
        const urlElement = document.getElementById('tunnelUrl');
        
        if (!statusElement || !urlElement) return;
        
        if (data.available) 
        {
            statusElement.textContent = 'Active';
            statusElement.style.color = 'green';
            urlElement.textContent = data.url;
            urlElement.style.color = 'green';
        } 
        else 
        {
            statusElement.textContent = 'Not available';
            statusElement.style.color = 'orange';
            urlElement.textContent = 'Direct connection only';
            urlElement.style.color = 'orange';
        }
    } 
    catch (error) 
    {
        console.error('Failed to check tunnel status:', error);
        
        const statusElement = document.getElementById('tunnelStatusText');
        const urlElement = document.getElementById('tunnelUrl');
        
        if (statusElement && urlElement) 
        {
            statusElement.textContent = 'Error';
            statusElement.style.color = 'red';
            urlElement.textContent = 'Failed to check status';
            urlElement.style.color = 'red';
        }
    }
}

// Обработка закрытия страницы - останавливаем стрим
window.addEventListener('beforeunload', () => {
    const streamer = window.asciiStreamer;
    if (streamer && streamer.isStreaming) 
    {
        // Отправляем команду остановки, но не ждем ответа
        if (streamer.ws && streamer.ws.readyState === WebSocket.OPEN) 
        {
            streamer.ws.send(JSON.stringify({
                type: 'stop_audio_capture'
            }));
            streamer.ws.send(JSON.stringify({ type: 'stop' }));
        }
    }
});