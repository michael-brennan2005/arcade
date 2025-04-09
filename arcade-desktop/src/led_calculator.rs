use eframe::egui::Color32;

// These are usizes because it makes doing the summing + averages a lot easier
#[derive(Debug, Clone, Copy)]
pub struct RGB {
    pub r: usize,
    pub g: usize,
    pub b: usize
}

impl Into<Color32> for RGB {
    fn into(self) -> Color32 {
        Color32::from_rgb(self.r as u8, self.g as u8, self.b as u8)
    }
}
// TODO: Find a better name to call this.
#[derive(Clone)]
pub struct LEDCalculator {
    screen_size: (usize, usize), // width x height in pixels
    pixels_per_cell: usize,
    pub grid: Vec<RGB>, // rows stored contiguously
}

impl LEDCalculator {
    pub fn new(width: usize, height: usize, pixels_per_cell: usize) -> Self {
        assert!(width % pixels_per_cell == 0);
        assert!(height % pixels_per_cell == 0);

        Self {
            screen_size: (width, height),
            pixels_per_cell,
            grid: vec![RGB { r: 0, g: 0, b: 0 }; ((width * height) / (pixels_per_cell * pixels_per_cell))],
        }
    }

    pub fn saturate(&mut self, x: usize, y: usize, r: usize, g: usize, b: usize) {
        let cell: &mut RGB = &mut self.grid[x / self.pixels_per_cell + (y / self.pixels_per_cell) * (self.screen_size.0 / self.pixels_per_cell)];
        
        cell.r += r;
        cell.g += g;
        cell.b += b;
    }

    pub fn average(&mut self) {
        let x = self.pixels_per_cell * self.pixels_per_cell;

        for cell in &mut self.grid {
            cell.r /= x;
            cell.g /= x;
            cell.b /= x;
        }
    }

    // TODO: vet and read
    pub fn write(&self, count: usize, buf: &mut Vec<u8>) {
        // Get perimeter
        let grid_width = self.screen_size.0 / self.pixels_per_cell;
        let grid_height = self.screen_size.1 / self.pixels_per_cell;
        let perimeter = 2 * (grid_width + grid_height);

        for i in 0..count {
            let idx = perimeter / count;
            
            // Calculate position along perimeter based on index
            let pos = i * idx;
            
            // Get coordinates based on position along perimeter
            let (x, y) = if pos < grid_width {
                // Top edge
                (pos, 0)
            } else if pos < grid_width + grid_height {
                // Right edge
                (grid_width - 1, pos - grid_width)
            } else if pos < 2 * grid_width + grid_height {
                // Bottom edge
                (2 * grid_width + grid_height - pos - 1, grid_height - 1)
            } else {
                // Left edge
                (0, 2 * (grid_width + grid_height) - pos - 1)
            };

            // Get grid cell at calculated position
            let cell = &self.grid[x + y * grid_width];
            
            // Write RGB values to buffer
            buf.push(cell.r as u8);
            buf.push(cell.g as u8); 
            buf.push(cell.b as u8);
        }
    }
}
