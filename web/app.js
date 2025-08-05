class AsciiStreamer {
    constructor() {
        this.output = document.getElementById('asciiOutput');
        this.ws = null;
        this.isStreaming = false;
        
        document.getElementById('startBtn').addEventListener('click', () => {
            if(this.isStreaming) this.stop();
            else this.start();
        });
    }

    start() {
        const resolution = document.getElementById('resolution').value;
        this.ws = new WebSocket(`ws://${window.location.host}/stream`);
        
        this.ws.onopen = () => {
            this.ws.send(JSON.stringify({ 
                type: 'config', 
                resolution: resolution 
            }));
            document.getElementById('startBtn').textContent = 'Stop Stream';
            this.isStreaming = true;
        };

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

    stop() {
        if(this.ws) {
            this.ws.close();
            this.ws = null;
        }
        document.getElementById('startBtn').textContent = 'Start Stream';
        this.isStreaming = false;
        this.output.textContent = "Stream stopped";
    }
}

window.addEventListener('load', () => new AsciiStreamer());