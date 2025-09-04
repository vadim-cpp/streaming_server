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
        this.loadCameras();
        
        // Обработчики кнопок API
        this.showApiBtn.addEventListener('click', () => this.showApiInfo());
        this.copyApiBtn.addEventListener('click', () => this.copyApiInfo());
        
        // Основная кнопка запуска стрима
        document.getElementById('startBtn').addEventListener('click', () => {
            if(this.isStreaming) this.stop();
            else this.start();
        });
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

    async start() 
    {
        if (this.isStreaming) return;

        this.updateUI(true);
        this.output.textContent = "Starting stream...";
        this.isStreaming = true;
        
        // Получаем API ключ перед созданием соединения
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
        };
        
        this.ws.onmessage = (event) => {
            // Убираем нулевой символ и пробелы
            const cleanedMessage = event.data.replace(/\u0000/g, '').trim();

            if (cleanedMessage === "AUTH_CONTROLLER_SUCCESS") 
            {
                const cameraIndex = document.getElementById('camera').value;
                const resolution = document.getElementById('resolution').value;
                const fps = 10;
                
                this.ws.send(JSON.stringify({
                    type: 'config',
                    camera_index: parseInt(cameraIndex),
                    resolution: resolution,
                    fps: fps
                }));
            } 
            else if (cleanedMessage === "CONFIG_APPLIED") 
            {
                this.output.textContent = "Stream started successfully";
            } 
            else if (cleanedMessage === "STREAM_STOPPED") 
            {
                // Сервер подтвердил остановку стрима
                this.updateUI(false);
            } 
            else 
            {
                this.output.textContent = cleanedMessage;
            }
        };

        this.ws.onclose = () => {
            if(this.isStreaming) this.stop();
        };

        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.stop();
        };
    }

    async stop() 
    {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) 
        {
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
    }

    updateUI(isStreaming) 
    {
        document.getElementById('startBtn').textContent = 
            isStreaming ? 'Stop Stream' : 'Start Stream';
        this.isStreaming = isStreaming;
        
        if (!isStreaming) 
        {
            this.output.textContent = "Stream stopped";
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
}

window.addEventListener('load', () => new AsciiStreamer());