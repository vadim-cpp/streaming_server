class AsciiStreamer 
{
    constructor() 
    {
        this.output = document.getElementById('asciiOutput');
        this.ws = null;
        this.isStreaming = false;

        this.cameraSelect = document.getElementById('camera');
        this.loadCameras();
        
        document.getElementById('startBtn').addEventListener('click', () => {
            if(this.isStreaming) this.stop();
            else this.start();
        });
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

    start() 
    {
        if (this.isStreaming) return;

        this.updateUI(true);
        this.output.textContent = "Starting stream...";
    
        this.isStreaming = true;
        document.getElementById('startBtn').textContent = 'Stop Stream';
        this.output.textContent = "Connecting...";

        const resolution = document.getElementById('resolution').value;
        const fps = 10; 
        
        this.ws = new WebSocket(`ws://${window.location.host}/stream`);

        const cameraIndex = document.getElementById('camera').value;
        
        this.ws.onopen = () => {
            this.ws.send(JSON.stringify({ 
                type: 'config',
                camera_index: parseInt(cameraIndex),
                resolution: resolution,
                fps: fps
            }));
        }

        this.ws.onmessage = (event) => {
            this.output.textContent = event.data;
        };

        this.ws.onclose = () => {
            if(this.isStreaming) this.stop();
        };

        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.stop();
        };
    }

    stop() 
    {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) 
        {
            // Отправляем команду остановки серверу
            this.ws.send(JSON.stringify({ type: 'stop' }));
            
            // Закрываем соединение после небольшой задержки
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
}

window.addEventListener('load', () => new AsciiStreamer());