include <BOSL2/std.scad>
include <BOSL2/shapes3d.scad>

// Diameter of pins (male)
Pin_diameter = 0.5;
// How far pins project from the base of a header into the thru holes
Pin_projection = 3;
// Height of the female-only portion of female headers
Female_header_base_height = 8.5;
// Height of the plastic retaining thing of male headers
Male_header_base_height = 2.6;
// Depth of the plastic retaining thing for headers
Header_base_depth = 2.5;
Male_header_total_height = 11.4;

PCB_height = 1.6;

Surface = 1.5;

/* [Self-tapping screws for the lid including tolerances (M2x6 are defaults)] */
Screw_height = 6.3;
Screw_head_diameter = 4.1;
Screw_head_height = 1.7;
Screw_shaft_diameter = 1.85;
// Distance inset from the corner
Screw_inset = 2;
	
Feather_width = 2 * 25.4;
Feather_depth = 0.9 * 25.4;

Extra_width = 11;
Extra_depth = 11;

Inner_height = 21;

Radius = 4;

Z_offset = -1.5;

USB_C_Z = 3.6;

$fn = 50;

module female_header(pin_count, pitch = 2.54) {
	assert(pin_count >= 1);
	
	for (i = [0 : pin_count - 1]) {
		translate([pitch * i, 0, -Pin_projection])
		cylinder(d = Pin_diameter, h = Pin_projection);
	}
	
	render()
	difference() {
		translate([-pitch / 2, -Header_base_depth / 2, 0])
		cube([pitch * pin_count, Header_base_depth, Female_header_base_height]);
	
		for (i = [0 : pin_count - 1]) {
			translate([pitch * i, 0, 0])
			cylinder(d = Pin_diameter + 0.2, h = Female_header_base_height);
		}
	}
}

module male_header(pin_count, pitch = 2.54) {
	assert(pin_count >= 1);
	
	for (i = [0 : pin_count - 1]) {
		translate([pitch * i, 0, 0])
		
		union() {
			cylinder(d = Pin_diameter, h = Male_header_total_height);
			
			translate([0, 0, -Male_header_base_height / 2 + Male_header_total_height - Pin_projection + pitch / 10])
			cuboid([pitch, Header_base_depth, Male_header_base_height], chamfer = pitch / 10);
		}
	}
}

// Feather specs: https://learn.adafruit.com/adafruit-feather/feather-specification
%color("#99ccff") {
	translate([6.35, 0.05 * 25.4, PCB_height])
	female_header(16);

	//translate([16.45, (0.9 * 25.4) - (0.05 * 25.4), PCB_height])
	//female_header(12);

	import("lib/5691 ESP32 S3 Reverse TFT Feather.stl");
}

%translate([8.9, -1.3, Female_header_base_height + Male_header_base_height + PCB_height])
rotate([0, 0, 0])
color("#ff99ff") {
	translate([2.5, 2.55, -Male_header_total_height + Pin_projection - 0.2])
	male_header(9);

	import("lib/4682 Micro SD Breakout.stl");
}

%translate([35.5, -1.3, PCB_height + Female_header_base_height])
color("#ffffaa") {
	translate([0, 0, Male_header_base_height])
	import("lib/3006 MAX98357.stl");
	translate([1.3, 2.55, -Male_header_total_height + Pin_projection + Male_header_base_height - 0.2])
	male_header(3);
}

%translate([24.5, 5, 4.5]) {
	cube([31, 17, 7.5]);
	
	translate([-5.6, 0, 7.5 - 4.2])
	cube([5.6, 17, 4.2]);
}

%translate([25, 28, 9])
rotate([90, 90, 0])
color("#99ffaa") import("lib/3923 Mini Oval Speaker.stl");

module usb_c_hole(depth) {
	$fn = 36;

	hole_width = 13;
	hole_radius = 4;

	for (y = [-hole_width / 2 + hole_radius, hole_width / 2 - hole_radius]) {
		translate([0, y, 0])
		rotate([90, 0, 90])
		cylinder(r = hole_radius, h = depth);
	}
	
	translate([depth / 2, 0, 0])
	cube([depth, hole_width - hole_radius * 2, hole_radius * 2], center = true);
}

module screw() {
	cylinder(d = Screw_shaft_diameter, h = Screw_height, $fn = 20);
	
	translate([0, 0, Screw_height - Screw_head_height])
	cylinder(d2 = Screw_head_diameter, d1 = Screw_shaft_diameter, h = Screw_head_height, $fn = 20);
}

module screws() {
	inset = 2;
	for (x = [-Extra_width / 2 + inset, 2 * 25.4 + Extra_width / 2 - inset]) {
		for (y = [-Extra_depth / 2 + inset, 0.9 * 25.4 + Extra_depth / 2 - inset]) {
			translate([x, y, Inner_height + Z_offset - Screw_height + Surface])
			screw();
		}
	}
}

module case() {
	difference() {
		translate([-(Extra_width / 2 - 1) / 2 - 1, (0.9 * 25.4) / 2, (USB_C_Z + 5) / 2 + Z_offset])
		cube([Extra_width / 2 - 1, 15, USB_C_Z + 8], center = true);
		
		translate([-(Surface + Extra_width / 2), (0.9 * 25.4) / 2, USB_C_Z])
		usb_c_hole(Surface + Extra_width / 2);
	}
	
	for (y = [0.1 * 25.4, (0.9 * 25.4) - (0.1 * 25.4)]) {
		translate([0.1 * 25.4, y, Z_offset])
		render()
		difference() {
			cylinder(d = 4, h = abs(Z_offset));
			cylinder(d = 2.35, h = abs(Z_offset));
		}
	}
	
	for (y = [1.85, (0.9 * 25.4) - 1.85]) {
		translate([(2 * 25.4) - (0.1 * 25.4), y, Z_offset])
		render()
		difference() {
			cylinder(d = 3.5, h = abs(Z_offset));
			cylinder(d = 1.85, h = abs(Z_offset));
		}
	}
	
	difference() {
		intersection() {
			translate([-Extra_width / 2 - Surface, -Extra_depth / 2 - Surface, -Surface + Z_offset])
			cuboid(
				[Feather_width + Extra_width + Surface * 2, Feather_depth + Extra_depth + Surface * 2, Inner_height + Surface],
				rounding = Radius,
				edges = ["Z"],
				anchor = [-1, -1, -1]
			);
			
			union() {
				for (x = [-Extra_width / 2 - Surface, 2 * 25.4 + Extra_width / 2 + Surface - 6]) {
					for (y = [-Extra_depth / 2 - Surface, 0.9 * 25.4 + Extra_depth / 2 + Surface - 6]) {
						translate([x, y, Inner_height - Surface - 4])
						cube([6, 6, 4]);
					}
				}
			}
		}
		
		screws();
	}
	
	translate([0, 0, Z_offset])
	render()
	difference() {
		translate([-Extra_width / 2 - Surface, -Extra_depth / 2 - Surface, -Surface])
		cuboid(
			[Feather_width + Extra_width + Surface * 2, Feather_depth + Extra_depth + Surface * 2, Inner_height + Surface],
			rounding = Radius,
			edges = ["Z"],
			anchor = [-1, -1, -1]
		);
	
		for (y = [0.1 * 25.4, (0.9 * 25.4) - (0.1 * 25.4)]) {
			translate([0.1 * 25.4, y, -Surface + 0.2])
			cylinder(d = 2.35, h = abs(Z_offset));
		}
	
		for (y = [1.85, (0.9 * 25.4) - 1.85]) {
			translate([(2 * 25.4) - (0.1 * 25.4), y, -Surface + 0.2])
			cylinder(d = 1.85, h = abs(Z_offset));
		}
	
		translate([-Extra_width / 2, -Extra_depth / 2, 0])
		cuboid(
			[Feather_width + Extra_width, Feather_depth + Extra_depth, Inner_height],
			rounding = Radius,
			edges = ["Z"],
			anchor = [-1, -1, -1]
		);
		
		translate([(2 * 25.4) / 2, (0.9 * 25.4) / 2, -Surface])
		prismoid(
			size2 = [24.91, 14.86],
			size1 = [24.91 + Surface * 2, 14.86 + Surface * 2],
			height = Surface
		);
		
		translate([44.45, (0.9 * 25.4) / 2, -Surface])
		cylinder(
			d = 4.2,
			h = Surface
		);
		
		translate([44.45, (0.9 * 25.4) / 2, -Surface + 0.2])
		cylinder(
			d = 6.2,
			h = Surface
		);
		
		for (y_delta = [-7, 0, 7]) {
			translate([7.6, (0.9 * 25.4) / 2 + y_delta, -Surface])
			union() {
				cylinder(
					d = 1.5,
					h = Surface
				);
				
				translate([0, 0, 0.2])
				cylinder(
					d = 5.5,
					h = Surface - 0.2
				);
			}
		}
		
		translate([Feather_width / 2, Feather_depth + Extra_depth + Surface - Extra_depth / 2, Inner_height / 2])
		rotate([90, 0, 0])
		union() {
			cylinder(d = 2, $fn = 6, h = Surface);
			
			for (i = [0 : 5]) {
				angle = (i / 6) * 360;
				translate([sin(angle) * 3, cos(angle) * 3, 0])
				cylinder(d = 2, $fn = 6, h = Surface);
			}
		}
		
		translate([-(Surface + Extra_width / 2), (0.9 * 25.4) / 2, USB_C_Z - Z_offset])
		usb_c_hole(Surface + Extra_width / 2);
		
		screws();
	}
}

module button() {
	translate([44.45, (0.9 * 25.4) / 2, -Surface + Z_offset - 1])
	cylinder(
		d = 4,
		h = Surface + 1.5
	);
	
	translate([44.45, (0.9 * 25.4) / 2, -Surface + Z_offset - 1 + 2.7])
	cylinder(
		d = 6,
		h = 0.3
	);
}

module backplate() {
	render()
	difference() {
		translate([-Extra_width / 2 - Surface, -Extra_depth / 2 - Surface, Inner_height - Surface])
		union() {
			cuboid(
				[Feather_width + Extra_width + Surface * 2, Feather_depth + Extra_depth + Surface * 2, Surface],
				rounding = Radius,
				edges = ["Z"],
				anchor = [-1, -1, -1]
			);
			
			for (y = [Extra_depth / 2 + 2 + 2, 0.9 * 25.4 + Extra_depth / 2 - 2]) {
				translate([Surface, y, -2])
				cube([Feather_width + Extra_width, 2, 2]);
			}
		}
		
		screws();
	}
}

//case();
//backplate();

button();
