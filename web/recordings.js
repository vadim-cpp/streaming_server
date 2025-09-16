class RecordingsManager 
{
    constructor() 
    {
        this.recordingsTable = document.getElementById('recordingsBody');
        this.refreshBtn = document.getElementById('refreshBtn');
        this.backBtn = document.getElementById('backBtn');
        this.playerPanel = document.getElementById('playerPanel');
        this.playBtn = document.getElementById('playBtn');
        this.pauseBtn = document.getElementById('pauseBtn');
        this.stopBtn = document.getElementById('stopBtn');
        this.speedSelect = document.getElementById('speedSelect');
        this.playbackOutput = document.getElementById('playbackOutput');
        this.currentTimeEl = document.getElementById('currentTime');
        this.totalTimeEl = document.getElementById('totalTime');
        
        this.refreshBtn.addEventListener('click', () => this.loadRecordings());
        this.backBtn.addEventListener('click', () => window.location.href = 'index.html');
        this.playBtn.addEventListener('click', () => this.startPlayback());
        this.pauseBtn.addEventListener('click', () => this.togglePause());
        this.stopBtn.addEventListener('click', () => this.stopPlayback());
        this.speedSelect.addEventListener('change', () => this.changePlaybackSpeed());
        
        this.currentRecording = null;
        this.isPlaying = false;
        this.isPaused = false;
        this.ws = null;
        
        this.loadRecordings();
    }
    
    async loadRecordings() 
    {
        try 
        {
            const response = await fetch('/recordings');
            const recordings = await response.json();
            this.populateRecordingsTable(recordings);
        } 
        catch (error) 
        {
            console.error('Failed to load recordings:', error);
            alert('Failed to load recordings list');
        }
    }
    
    populateRecordingsTable(recordings) 
    {
        this.recordingsTable.innerHTML = '';
        
        if (recordings.length === 0) 
        {
            const row = document.createElement('tr');
            row.innerHTML = '<td colspan="5" style="text-align: center;">No recordings available</td>';
            this.recordingsTable.appendChild(row);
            return;
        }
        
        // Sort by date (newest first)
        recordings.sort((a, b) => b.last_modified - a.last_modified);
        
        recordings.forEach(recording => {
            const row = document.createElement('tr');
            
            // Format date
            const date = recording.timestamp || new Date(recording.last_modified).toLocaleString();
            
            // Format size
            const size = this.formatFileSize(recording.size);
            
            // Format duration
            const duration = recording.duration ? this.formatDuration(recording.duration) : 'N/A';
            
            row.innerHTML = `
                <td>${recording.filename}</td>
                <td>${date}</td>
                <td>${duration}</td>
                <td>${size}</td>
                <td>
                    <button class="play-btn" data-filename="${recording.filename}">Play</button>
                    <button class="delete-btn" data-filename="${recording.filename}">Delete</button>
                </td>
            `;
            
            this.recordingsTable.appendChild(row);
        });
        
        // Add event listeners to buttons
        document.querySelectorAll('.play-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                this.selectRecording(e.target.dataset.filename);
            });
        });
        
        document.querySelectorAll('.delete-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                this.deleteRecording(e.target.dataset.filename);
            });
        });
    }
    
    formatFileSize(bytes) 
    {
        if (bytes < 1024) return bytes + ' B';
        else if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(2) + ' KB';
        else return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
    }
    
    formatDuration(seconds) 
    {
        const mins = Math.floor(seconds / 60);
        const secs = seconds % 60;
        return `${mins}:${secs.toString().padStart(2, '0')}`;
    }
    
    async selectRecording(filename) 
    {
        this.currentRecording = filename;
        this.playerPanel.style.display = 'block';
        this.playbackOutput.textContent = 'Select Play to start playback';
        this.updatePlaybackControls();
    }
    
    async deleteRecording(filename) 
    {
        if (!confirm(`Are you sure you want to delete "${filename}"?`)) 
        {
            return;
        }
        
        try 
        {
            const response = await fetch(`/recordings/${filename}`, {
                method: 'DELETE'
            });
            
            if (response.ok) 
            {
                alert('Recording deleted successfully');
                this.loadRecordings();
            } 
            else 
            {
                alert('Failed to delete recording');
            }
        } 
        catch (error) 
        {
            console.error('Delete error:', error);
            alert('Error deleting recording');
        }
    }
    
    startPlayback() 
    {
        if (!this.currentRecording) 
        {
            alert('No recording selected');
            return;
        }
        
        // Connect to WebSocket for playback
        this.ws = new WebSocket(`wss://${window.location.host}/playback`);
        
        this.ws.onopen = () => {
            this.ws.send(JSON.stringify({
                type: 'playback_start',
                filename: this.currentRecording
            }));
            
            this.isPlaying = true;
            this.isPaused = false;
            this.updatePlaybackControls();
        };
        
        this.ws.onmessage = (event) => {
            this.playbackOutput.textContent = event.data;
        };
        
        this.ws.onclose = () => {
            this.isPlaying = false;
            this.isPaused = false;
            this.updatePlaybackControls();
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            alert('Playback error');
            this.isPlaying = false;
            this.isPaused = false;
            this.updatePlaybackControls();
        };
    }
    
    togglePause() 
    {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) 
        {
            return;
        }
        
        if (this.isPaused) 
        {
            this.ws.send(JSON.stringify({ type: 'playback_resume' }));
            this.isPaused = false;
        } 
        else 
        {
            this.ws.send(JSON.stringify({ type: 'playback_pause' }));
            this.isPaused = true;
        }
        
        this.updatePlaybackControls();
    }
    
    stopPlayback() 
    {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) 
        {
            this.ws.send(JSON.stringify({ type: 'playback_stop' }));
            this.ws.close();
        }
        
        this.isPlaying = false;
        this.isPaused = false;
        this.updatePlaybackControls();
        this.playbackOutput.textContent = 'Playback stopped';
    }
    
    changePlaybackSpeed() 
    {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) 
        {
            return;
        }
        
        const speed = parseFloat(this.speedSelect.value);
        this.ws.send(JSON.stringify({
            type: 'playback_speed',
            speed: speed
        }));
    }
    
    updatePlaybackControls() 
    {
        this.playBtn.disabled = this.isPlaying;
        this.pauseBtn.disabled = !this.isPlaying;
        this.stopBtn.disabled = !this.isPlaying;
        this.speedSelect.disabled = !this.isPlaying;
        
        if (this.isPaused) 
        {
            this.pauseBtn.textContent = 'Resume';
        } 
        else 
        {
            this.pauseBtn.textContent = 'Pause';
        }
    }
}

// Initialize when page loads
window.addEventListener('load', () => new RecordingsManager());