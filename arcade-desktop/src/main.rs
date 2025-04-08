use std::thread;
use std::time::Instant;

use audio::{VolumeData, VolumeMonitor};
use circular_queue::CircularQueue;
use cpal::{traits::{DeviceTrait, HostTrait, StreamTrait}, StreamConfig};
use eframe::egui::{self, Pos2, Vec2};
use led_calculator::LEDCalculator;
use serialport::{SerialPort, SerialPortInfo, SerialPortType};
use windows_capture::{
    capture::GraphicsCaptureApiHandler,
    monitor::Monitor,
    settings::{ColorFormat, CursorCaptureSettings, DrawBorderSettings, Settings},
};

mod audio;
mod capture;
mod led_calculator;

use capture::Capture;
use rtrb::{Consumer, RingBuffer};

fn main() {
    // For capture thread <-> app thread
    let (led_producer, led_consumer) = RingBuffer::<led_calculator::LEDCalculator>::new(256);
    let (mut audio_producer, audio_consumer) = RingBuffer::<VolumeData>::new(1);

    // Gets the foreground window, refer to the docs for other capture items
    let primary_monitor = Monitor::primary().expect("There is no primary monitor");

    let settings = Settings::new(
        primary_monitor,
        CursorCaptureSettings::WithoutCursor,
        DrawBorderSettings::WithoutBorder,
        ColorFormat::Rgba8,
        led_producer, // Abusing the flags setup to pass in the producer here
    );

    // Windows capture need its own thread
    thread::spawn(|| {
        Capture::start(settings).expect("Screen capture failed");
    });

    // ^^^^ Screen capture, VVV audio capture
    let host = cpal::default_host();
    let device = host.default_output_device().unwrap();
    let device_config = device.default_output_config().unwrap();

    let mut volume_monitor = VolumeMonitor::new(0.9, 0.1, 30, 0.05, 1.3);

    let stream = device
        .build_input_stream(
            &device_config.into(), 
            move |data: &[f32], _: &cpal::InputCallbackInfo| {
                let data = volume_monitor.process_frame(data);
                audio_producer.push(data);
            },
            move |err| {
                panic!("Audio input stream failure: {}", err);
            },
            None
        )
        .expect("Input stream build failed");
    stream.play().expect("Stream playing failed");

    let native_options = eframe::NativeOptions::default();
    let _ = eframe::run_native(
        "My egui App",
        native_options,
        Box::new(|cc| Ok(Box::new(ClientApp::new(led_consumer, audio_consumer, cc)))),
    );
}

// Oh my god
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum PortConnectionStatus {
    NoPortSelected,
    ConnectionSuccessful,
    ConnectionFailed,
    WritingFailed
}

impl ToString for PortConnectionStatus {
    fn to_string(&self) -> String {
        match self {
            PortConnectionStatus::ConnectionSuccessful => String::from("Connection success!"),
            PortConnectionStatus::ConnectionFailed => String::from("Connection failed"),
            PortConnectionStatus::WritingFailed => String::from("Writing failed"),
            PortConnectionStatus::NoPortSelected => String::from("No port selected"),
        }
    }
}

struct ClientApp {
    led_consumer: Consumer<LEDCalculator>,
    last_capture: Option<LEDCalculator>,
    time_since_fresh_capture: Instant,

    audio_consumer: Consumer<VolumeData>,

    port: Option<Box<dyn SerialPort>>,
    selected_port_info: Option<SerialPortInfo>, // For the UI - check port for what's the actual port
    port_connection_status: PortConnectionStatus,

    led_count: usize 
}

impl ClientApp {
    fn new(
        led_consumer: Consumer<LEDCalculator>, 
        audio_consumer: Consumer<VolumeData>, 
        _: &eframe::CreationContext<'_>) -> Self {
        // Customize egui here with cc.egui_ctx.set_fonts and cc.egui_ctx.set_visuals.
        // Restore app state using cc.storage (requires the "persistence" feature).
        // Use the cc.gl (a glow::Context) to create graphics shaders and buffers that you can use
        // for e.g. egui::PaintCallback.
        Self { 
            led_consumer, 
            audio_consumer,
            last_capture: None, 
            time_since_fresh_capture: Instant::now(), 
            port: None,
            port_connection_status: PortConnectionStatus::NoPortSelected,
            selected_port_info: None,
            led_count: 25
        }
    }
}

impl eframe::App for ClientApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let mut current_capture = match self.led_consumer.pop() {
            Ok(val) => {
                self.last_capture = Some(val.clone());
                self.time_since_fresh_capture = Instant::now();
                val
            },
            Err(_) => match &self.last_capture {
                Some(val) => val.clone(),
                None => return, // should only happen like once i think
            },
        };

        current_capture.average();
        
        let current_volume = match self.audio_consumer.pop() {
            Ok(val) => val,
            Err(_) => VolumeData { peak_level: 0.0, average_level: 0.0, should_trigger: false }
        };

        let options: Vec<SerialPortInfo> = serialport::available_ports().unwrap_or(vec![]);
        
        egui::CentralPanel::default().show(ctx, |ui| {
            ui.label(format!("{}", current_volume));
            ui.label(format!("Time since last frame: {:?}", 
                (Instant::now() - self.time_since_fresh_capture)));
            
            let combo_box_text = match &self.selected_port_info {
                Some(info) => &info.port_name,
                None => "No port selected",
            };

            egui::ComboBox::from_label("Select ESP32 port")
                .selected_text(format!("{}", combo_box_text))
                .show_ui(ui, |ui| {
                    for option in &options {
                        let option_text = match &option.port_type {
                            SerialPortType::UsbPort(info) => {
                                format!("{} | {} | {}", 
                                    option.port_name.clone(), 
                                    info.manufacturer.clone().unwrap_or(String::from("N/A")), 
                                    info.product.clone().unwrap_or(String::from("N/A")))
                            },
                            _ => option.port_name.clone()
                        };

                        ui.selectable_value(
                            &mut self.selected_port_info,
                            // why tf are they using strings ?!?!?! IM FINNA CRASH OUT!!!!
                            // i think this is actaully best practice but mannnn
                            // i dont like clones :( 
                            Some(option.clone()), 
                            option_text);
                    }
                });
            
            ui.horizontal(|ui| {
                if ui.button("Connect").clicked() {
                    match &self.selected_port_info {
                        Some(info) => match serialport::new(&info.port_name, 115_200).open() {
                            Ok(port) => {
                                self.port_connection_status = PortConnectionStatus::ConnectionSuccessful;
                                self.port = Some(port);
                            },
                            Err(_) => {
                                self.port_connection_status = PortConnectionStatus::ConnectionFailed
                            },
                        },
                        None => {
                            self.port_connection_status = PortConnectionStatus::NoPortSelected
                        },
                    }
                }

                ui.label(self.port_connection_status.to_string());
            });

            ui.add(egui::Slider::new(&mut self.led_count, 1..=100).text("LED count"));

            for (i, cell) in current_capture.grid.iter().enumerate() {
                let side_length = 25.0;
                let x = (i % (1920 / 120)) as f32;
                let y = (i / (1920 / 120)) as f32;

                ui.painter().rect_filled(
                    egui::Rect::from_min_size(
                        Pos2::new(x * side_length, y * side_length + 200.0),
                        Vec2::new(side_length, side_length)),
                        0,
                        *cell
                    );
                }
        });
        
        if let Some(port) = &mut self.port {
            // Would make this an AND with the parent if but that's 'unstable' with let expressions
            // rn
            if self.port_connection_status != PortConnectionStatus::WritingFailed {           
                let mut buf: Vec<u8> = vec![0; 1 + self.led_count * 3];
                buf[0] = 0x77;
                for i in 0..self.led_count {
                    buf[1 + i * 3] = current_capture.grid[i].r as u8;
                    buf[1 + i * 3 + 1] = current_capture.grid[i].g as u8;
                    buf[1 + i * 3 + 2] = current_capture.grid[i].b as u8;
                }

                // TODO: we probably wont be sending buffers large enough that they wont be fully
                // written on one call. if there ever seems to be reliability/data issues though,
                // would be good to start here.
                match port.write(buf.as_slice()) {
                    Ok(_) => {},
                    Err(_) => {
                        self.port_connection_status = PortConnectionStatus::WritingFailed
                    },
                }

                
            } 
        }
        
        ctx.request_repaint();
    }
}
