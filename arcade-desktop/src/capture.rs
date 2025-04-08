use std::time::Instant;

use windows_capture::{
    capture::{Context, GraphicsCaptureApiHandler},
    frame::Frame,
    graphics_capture_api::InternalCaptureControl,
};

use crate::led_calculator::LEDCalculator;

const PIXELS_PER_CELL: usize = 120;

// Handles capture events.
pub struct Capture {
    producer: rtrb::Producer<LEDCalculator>
}

impl GraphicsCaptureApiHandler for Capture {
    // The type of flags used to get the values from the settings.
    type Flags = rtrb::Producer<LEDCalculator>;

    // The type of error that can be returned from `CaptureControl` and `start` functions.
    type Error = Box<dyn std::error::Error + Send + Sync>;

    // Function that will be called to create a new instance. The flags can be passed from settings.
    fn new(ctx: Context<Self::Flags>) -> Result<Self, Self::Error> {
        Ok(Self {
            producer: ctx.flags
        })
    }

    // Called every time a new frame is available.
    fn on_frame_arrived(
        &mut self,
        frame: &mut Frame,
        _capture_control: InternalCaptureControl,
    ) -> Result<(), Self::Error> {
        let frame_width = frame.width() as usize;
        let frame_height = frame.height() as usize;

        let mut calculator = LEDCalculator::new(
            frame_width, 
            frame_height, 
            PIXELS_PER_CELL);

        let mut data = frame.buffer()?;
        let data = data.as_nopadding_buffer()?;
        
        // let start = Instant::now();
            
        let mut i = 0;
        while i < data.len() {
            let j = i / 4;
            calculator.saturate(
                j % frame_width, j / frame_width, data[i + 0] as usize, data[i + 1] as usize, data[i + 2] as usize
            );
            i += 4;
        }
    
        // let end = Instant::now();
        // println!("{:?}", end - start);
        
        self.producer.push(calculator);
        
        Ok(())
    }

    // Optional handler called when the capture item (usually a window) closes.
    fn on_closed(&mut self) -> Result<(), Self::Error> {
        println!("Capture session ended");

        Ok(())
    }
}