use std::{collections::VecDeque, fmt::Display};

/// Thanks Claude! This does audio processing idk
pub struct VolumeMonitor {
    // Peak detection parameters
    current_peak: f32,
    attack_rate: f32,
    release_rate: f32,
    
    // Average level tracking
    average_level: f32,
    recent_volumes: VecDeque<f32>,
    history_capacity: usize,
    
    // Trigger parameters
    threshold: f32,
    peak_multiplier: f32,
    
    // Last calculated values
    last_trigger_state: bool,
}

impl VolumeMonitor {
    /// Create a new VolumeMonitor with specified parameters
    pub fn new(
        attack_rate: f32,
        release_rate: f32,
        history_size: usize,
        threshold: f32,
        peak_multiplier: f32,
    ) -> Self {
        Self {
            current_peak: 0.0,
            attack_rate,
            release_rate,
            average_level: 0.0,
            recent_volumes: VecDeque::with_capacity(history_size),
            history_capacity: history_size,
            threshold,
            peak_multiplier,
            last_trigger_state: false,
        }
    }
    
    /// Create a new VolumeMonitor with sensible defaults
    pub fn with_defaults() -> Self {
        Self::new(0.8, 0.05, 50, 0.1, 1.5)
    }
    
    /// Process a frame of audio samples and update all internal state
    pub fn process_frame(&mut self, samples: &[f32]) -> VolumeData {
        // Calculate RMS for current frame
        let rms = self.calculate_rms(samples);
        
        // Update peak detection
        self.update_peak(rms);
        
        // Update average level tracking
        self.update_average(rms);
        
        // Check if we should trigger
        self.last_trigger_state = self.should_trigger();
        
        // Return the current state
        VolumeData {
            peak_level: self.current_peak,
            average_level: self.average_level,
            should_trigger: self.last_trigger_state,
        }
    }
    
    /// Calculate Root Mean Square of samples
    fn calculate_rms(&self, samples: &[f32]) -> f32 {
        if samples.is_empty() {
            return 0.0;
        }
        
        let sum_squared: f32 = samples.iter().map(|&s| s * s).sum();
        (sum_squared / samples.len() as f32).sqrt()
    }
    
    /// Update peak detection with new sample
    fn update_peak(&mut self, value: f32) {
        if value > self.current_peak {
            // Fast attack
            self.current_peak = self.current_peak * (1.0 - self.attack_rate) + 
                              value * self.attack_rate;
        } else {
            // Slow release
            self.current_peak = self.current_peak * (1.0 - self.release_rate);
        }
    }
    
    /// Update average level tracking
    fn update_average(&mut self, value: f32) {
        // Add to history
        if self.recent_volumes.len() >= self.history_capacity {
            self.recent_volumes.pop_front();
        }
        self.recent_volumes.push_back(value);
        
        // Recalculate average
        if !self.recent_volumes.is_empty() {
            self.average_level = self.recent_volumes.iter().sum::<f32>() / 
                              self.recent_volumes.len() as f32;
        }
    }
    
    /// Check if current levels should trigger haptic feedback
    fn should_trigger(&self) -> bool {
        self.current_peak > self.threshold * self.peak_multiplier || 
        self.average_level > self.threshold
    }
}

/// Structure to hold volume data for sharing between threads
pub struct VolumeData {
    pub peak_level: f32,
    pub average_level: f32,
    pub should_trigger: bool,
}

impl Display for VolumeData {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Peak level: {:.2}, average level: {:.2}, should trigger: {}", 
        self.peak_level, 
        self.average_level, 
        self.should_trigger)
    }
}