include <BOSL2/std.scad>

// All units are mm

/* [Components] */
// Width of the entire LCD board
LCD_board_width = 57.15;
// Depth of the entire LCD board
LCD_board_depth = 36.83;
// Depth that the screw tabs protrude from the board's main body
LCD_screw_tab_depth = 5.08;
// X/Y inset of the screw holes for the LCD
LCD_screw_inset = 2.55;
// Height of the top of the LCD to the board's top
LCD_display_height = 1.5;
// Height of the entire LCD board, including the display
LCD_height = 4.97;
// Viewable width of the LCD
LCD_viewable_width = 43;
// Viewable depth of the LCD
LCD_viewable_depth = 22;
// Width of the Feather
Feather_width = 2 * 25.4;
// Depth of the Feather
Feather_depth = 22.86;
// Height of the Feather
Feather_height = 6.37;
// Speaker width
Speaker_width = 20;
// Speaker depth
Speaker_depth = 30;
// Speaker height
Speaker_height = 5;
// Battery_width
Battery_width = 35;
// Battery_depth
Battery_depth = 16;
// Battery_height
Battery_height = 7.8;
// Battery X inset from center, origin is left edge of battery
Battery_x_offset = -20;
// Speaker X offset
Speaker_x_offset = 9;
// I2S audio amplifier width
Audio_amp_width = 17.78;
// I2S audio amplifier depth
Audio_amp_depth = 19.05;
// I2S audio amplifier Z offset, which is implicitly the standoff height too
Audio_amp_z_offset = 2.5;
// I2S audio amplifier Y offset
Audio_amp_y_offset = -1;
// Width of the button board, not just the round button
Button_board_width = 20.8;
// Depth of the button board, not just the round button
Button_board_depth = 27.8;
// Total height of the button board, including the top of the round button
Button_total_height = 14.13;
// Y offset from the center of the button board to the center of the round button
Button_y_offset = -5;
// Z projection of the round up to, but excluding, the lip of the plastic button
Button_z_projection = 4.3;
// Diameter of the top of the plastic button (not the lip)
Button_diameter = 11.4;
// Z offset of the button board
Button_z_offset = 1;
// X inset (once positioned) of the button's standoffs from the board's edge
Button_x_standoff_inset = 2.4;
// Y inset (once positioned) of the button's standoffs from the board's edge
Button_y_standoff_inset = 3;
// Feather reset button's X offset (origin is the center edge by the USB C port)
Feather_reset_x_offset = 10.8;
// Feather reset button's Y offset (origin is the center edge by the USB C port)
Feather_reset_y_offset = 6.3;
// Feather USB C Z delta
Feather_USB_C_z_delta = 3.2;
// Height of the Feather's PCB only
Feather_PCB_height = 1.6;
// X offset from the Feather board's corner of the retaining pin
Feather_retaining_pin_x_delta = 1.8;
// Y offset from the Feather board's corner of the retaining pin
Feather_retaining_pin_y_delta = 1.8;
// X offset from the Feather board's corner of the retaining pin
Feather_standoff_x_delta = 2.5;
// Y offset from the Feather board's corner of the retaining pin
Feather_standoff_y_delta = 2.5;

/* [Case and standoff geometry] */
// Size of most surfaces
Surface = 1.5;
// Edge of the LCD to edge of the Feather and button board, no tolerance
Case_x_spacing = 16.1;
// Extra space to add to X/Y case inner space
Case_clearance = 1.5;
// Radius for case curvature
Case_radius = 10;
// Extra space around the button
Button_clearance = 1;
// Reset hole diameter (enough for a paperclip)
Reset_hole_diameter = 1.75;
// Height of the battery retaining tabs
Battery_retainer_height = 6;
// Standoff screw tolerance; standoffs are this much smaller than the screws they support. Doesn't apply to the self-tapping screws used for the lid.
Standoff_tolerance = -0.1;
// Radius to apply to standoff walls
Standoff_wall_radius = 1.5;

/* [Tolerances] */
// Feather extra Z offset from the LCD
Feather_z_offset = 1;
// Battery extra Z offset from the Feather (may be negative as parts of the Feather are shorter)
Battery_z_offset = 0;
// Button X offset from the edge of the LCD board
Button_x_offset = 1.5;

/* [Self-tapping screws for the lid including tolerances (M2x6 are defaults)] */
Screw_height = 6.3;
Screw_head_diameter = 4.1;
Screw_head_height = 1.7;
Screw_shaft_diameter = 1.7;
// Distance inset from the corner
Screw_inset = 2;

module __Customizer_Limit__ () {}

$fs = 0.1;
$fa = 1;

function inner_height() = LCD_height + Feather_z_offset + Feather_height + Battery_z_offset + Battery_height;
function inner_width() = LCD_board_width + Case_x_spacing * 2 + Case_clearance;
function inner_depth() = LCD_board_depth + Case_clearance;

module lcd() {
	translate([-LCD_board_width / 2, LCD_board_depth / 2 - LCD_screw_tab_depth, -LCD_display_height])
	rotate([180, 0, 0])
	import("lib/5394 1.9in TFT Display.stl");
}

module feather() {
	translate([
		-LCD_board_width / 2 - Case_x_spacing,
		Feather_depth / 2,
		-LCD_height - Feather_z_offset
	])
	rotate([180, 0, 0])
	import("lib/5323 Feather ESP32-S3.stl");
}

module speaker() {
	translate([Speaker_x_offset, LCD_board_depth / 2, -Speaker_width / 2])
	rotate([90, 270, 0])
	import("lib/3923 Mini Oval Speaker.stl");
}

module battery() {
	translate([
		Battery_x_offset,
		-LCD_board_depth / 2,
		-Battery_height - LCD_height - Feather_height - Feather_z_offset - Battery_z_offset
	])
	cube([Battery_width, Battery_depth, Battery_height]);
}

module audio_amp() {
	translate([
		-Speaker_depth / 2 + Speaker_x_offset - Audio_amp_width,
		-Audio_amp_depth + LCD_board_depth / 2 + Audio_amp_y_offset,
		-inner_height() + Audio_amp_z_offset
	])
	import("lib/3006 MAX98357.stl");
}

module button() {
	// move the button to the right edge
	translate([LCD_board_width / 2 + Button_diameter / 2 + Button_x_offset, 0, -Button_z_offset])
	rotate([0, 0, -90])
	// this translation centers the board at the center of the round button and with it protruding from the lip upwards
	translate([
		-Button_board_width / 2,
		-Button_board_depth / 2 + Button_y_offset,
		-Button_total_height + Button_z_projection
	])
	import("lib/4431 STEMMA Buttons.stl");
}

module standoffs() {
	// audio amp
	inset = 2.5;
	for (x = [inset, Audio_amp_width - inset]) {
		translate([
			-Speaker_depth / 2 + Speaker_x_offset - Audio_amp_width + x,
			-Audio_amp_depth + LCD_board_depth / 2 + Audio_amp_depth - inset + Audio_amp_y_offset,
			-inner_height()
		])
		difference() {
			cylinder(d = 2.5 + Standoff_tolerance + Standoff_wall_radius * 2, h = Audio_amp_z_offset);
			cylinder(d = 2.5 + Standoff_tolerance, h = Audio_amp_z_offset);
		}
	}
	
	// Feather retaining pin by ESP32
	retaining_pin_base = 4.5;
	translate([
		-inner_width() / 2 + Feather_width - retaining_pin_base / 2 - Feather_retaining_pin_x_delta,
		Feather_depth / 2 - retaining_pin_base / 2 - Feather_retaining_pin_y_delta,
		-inner_height()
	])
	cube([retaining_pin_base, retaining_pin_base, inner_height() - LCD_height - Feather_z_offset - Feather_PCB_height]);
	
	translate([
		-inner_width() / 2 + Feather_width - Feather_retaining_pin_x_delta,
		Feather_depth / 2 - Feather_retaining_pin_y_delta,
		-LCD_height - Feather_z_offset - Feather_PCB_height
	])
	cylinder(d = 2 + Standoff_tolerance, h = Feather_PCB_height + 1);
	
	// Floating LCD standoffs
	lcd_screw_hole_size = 2.5;
	floating_height = 3;
	height = LCD_height - LCD_display_height;
	for (x = [-LCD_board_width / 2 + LCD_screw_inset, LCD_board_width / 2 - LCD_screw_inset]) {
		for (y = [-LCD_board_depth / 2 + LCD_screw_inset, LCD_board_depth / 2 - LCD_screw_inset]) {
			y_delta = (y < 0 ? -Case_clearance : Case_clearance) / 2;
			depth = lcd_screw_hole_size + floating_height + abs(y_delta); 
			translate([x, y + y_delta, -height - floating_height / 2])
			difference() {
				union() {
					cube([
						lcd_screw_hole_size + floating_height,
						lcd_screw_hole_size + floating_height + Case_clearance / 2,
						floating_height
					], center = true);
					
					translate([0, -depth / 2 + (y_delta > 0 ? depth : 0), -height - floating_height - 0.5]) // FIXME this 0.5 is stupid
					prismoid(
						size1 = [lcd_screw_hole_size + floating_height, 0],
						size2 = [lcd_screw_hole_size + floating_height, depth],
						h = lcd_screw_hole_size + floating_height,
						shift = [0, depth / 2 * (y_delta > 0 ? -1 : 1)]
					);
				}
				
				translate([0, -y_delta, -floating_height / 2])
				cylinder(d = lcd_screw_hole_size - Standoff_tolerance, h = floating_height);
			}
		}
	}
	
	// Floating Feather standoffs by USB port
	width_delta = inner_width() / 2 + (-LCD_board_width / 2 - Case_x_spacing);
	base_xy = 4.5;
	base_z = 3;
	
	difference() {
		union() {
			for (y = [Feather_depth / 2 - base_xy, -Feather_depth / 2]) {
				translate([-inner_width() / 2, y, -LCD_height - Feather_z_offset - base_z - Feather_PCB_height])
				cube([base_xy + width_delta, base_xy, 3]);
			}
			
			for (y = [Feather_depth / 2 - base_xy / 2, -Feather_depth / 2 + base_xy / 2]) {
				translate([-inner_width() / 2, y, -LCD_height - Feather_z_offset - base_z - Feather_PCB_height - base_xy])
				prismoid(
					size1 = [0, base_xy],
					size2 = [base_xy + width_delta, base_xy],
					h = base_xy,
					shift = [(base_xy + width_delta) / 2, 0]
				);
			}
		}
	
		for (y = [Feather_depth / 2 - Feather_standoff_y_delta, -Feather_depth / 2 + Feather_standoff_y_delta]) {
			translate([
				-LCD_board_width / 2 - Case_x_spacing + Feather_standoff_x_delta,
				y,
				-LCD_height - Feather_z_offset - base_z - Feather_PCB_height
			])
			cylinder(d = 2.5 + Standoff_tolerance, h = 3);
		}
	}
	
	// Button standoffs
	button_board_x = LCD_board_width / 2 + Button_diameter / 2 + Button_x_offset + Button_y_offset - Button_board_depth / 2;
	button_board_hole_size = 2.5;
	button_height = inner_height() - Button_z_offset - Button_total_height + Button_z_projection;
	for (y = [Button_board_width / 2 - Button_y_standoff_inset, -Button_board_width / 2 + Button_y_standoff_inset]) {
		translate([button_board_x + Button_x_standoff_inset, y, -inner_height()])
		render()
		difference() {
			cylinder(d = button_board_hole_size + Standoff_tolerance + Standoff_wall_radius * 2, h = button_height);
			cylinder(d = button_board_hole_size + Standoff_tolerance, h = button_height);
		}
	}
	
	// Button lip on the edge of the case
	lip_size = 2.5;
	translate([inner_width() / 2, 0, -lip_size - inner_height() + button_height])
	prismoid(
		size1 = [0, Button_board_width],
		size2 = [lip_size, Button_board_width],
		h = lip_size,
		shift = [-lip_size / 2, 0]
	);
		
	// battery retaining lips
	translate([Battery_x_offset + 15, -LCD_board_depth / 2 + Battery_depth, -inner_height()])
	cube([Battery_width - 15, 1, Battery_retainer_height]);
	
	translate([Battery_x_offset - 1, -LCD_board_depth / 2 + 10 / 2, -inner_height()])
	cube([1, Battery_depth - 10, Battery_retainer_height]);
}

module usb_c_hole(depth) {
	hole_width = 10.5;
	hole_radius = 2;

	for (y = [-hole_width / 2 + hole_radius, hole_width / 2 - hole_radius]) {
		translate([0, y, 0])
		rotate([90, 0, 90])
		cylinder(r = hole_radius, h = depth);
	}
	
	translate([depth / 2, 0, 0])
	cube([depth, hole_width - hole_radius * 2, hole_radius * 2], center = true);
}

module case() {
	outer_width = inner_width() + Surface * 2;
	outer_depth = inner_depth() + Surface * 2;
	outer_height = inner_height() + Surface;
	
	reset_guide_height = inner_height() - LCD_height - Feather_z_offset - Feather_height;
	reset_guide_origin = [
		-LCD_board_width / 2 - Case_x_spacing + Feather_reset_x_offset,
		Feather_reset_y_offset,
		-inner_height()
	];
	
	// lid screw holes
	difference() {
		intersection() {
			union() {
				size = Case_radius - Screw_inset;
				for (x = [
					inner_width() / 2,
					-inner_width() / 2
				]) {
					for (y = [
						inner_depth() / 2,
						-inner_depth() / 2
					]) {
						translate([x + (x > 0 ? -size : 0), y + (y > 0 ? -size : 0), -Screw_height / 2])
						cube([size, size, Screw_height / 2]);
					
						x_shift_multiplier = x < 0 ? 1 : -1;
						y_shift_multiplier = y < 0 ? 1 : -1;

						translate([x, y, -size - Screw_height / 2])
						prismoid(
							size1 = [0, 0],
							size2 = [size, size],
							h = size,
							shift = [size / 2 * x_shift_multiplier, size / 2 * y_shift_multiplier]
						);
					}
				}
			}
			
			translate([0, 0, -outer_height / 2])
			cuboid(
				[outer_width, outer_depth, outer_height],
				rounding = Case_radius,
				edges = ["Z"]
			);
		}
		
		screws();
	}

	render()
	difference() {
		union() {
			// main case body
			difference() {
				translate([0, 0, -outer_height / 2])
				cuboid(
					[outer_width, outer_depth, outer_height],
					rounding = Case_radius,
					edges = ["Z"]
				);
			
				translate([0, 0, -inner_height() / 2])
				cuboid(
					[inner_width(), inner_depth(), inner_height()],
					rounding = Case_radius,
					edges = ["Z"]
				);				
			}
				
			// reset hole guide (not the hole itself)
			translate(reset_guide_origin)
			cylinder(d = Reset_hole_diameter + 2, h = reset_guide_height);
			
			// standoffs (obviously)
			standoffs();
		}

		// USB C hole
		translate([
			-inner_width() / 2 - Surface,
			0, 
			-LCD_height - Feather_z_offset - Feather_USB_C_z_delta
		])
		usb_c_hole(Surface);

		// speaker holes
		translate([Speaker_x_offset, inner_depth() / 2, -Speaker_width / 2])
		rotate([0, 90, 90])
		union() {
			cylinder(d = 1.5, h = Surface);
			for (i = [0 : 5]) {
				angle = (i / 6) * 360;
				x = sin(angle) * 3;
				y = cos(angle) * 3;
				
				translate([x, y, 0])
				cylinder(d = 1.5, h = Surface);
			}
		}

		// reset hole
		translate([0, 0, -Surface])
		translate(reset_guide_origin)
		cylinder(d = Reset_hole_diameter, h = reset_guide_height + Surface);
	}
}

module lid() {
	outer_width = inner_width() + Surface * 2;
	outer_depth = inner_depth() + Surface * 2;
	
	render()
	difference() {
		// main body
		translate([0, 0, -Surface / 2 + Surface])
		cuboid(
			[outer_width, outer_depth, Surface],
			rounding = Case_radius,
			edges = ["Z"]
		);
		
		// button hole
		translate([
			LCD_board_width / 2 + Button_diameter / 2 + Button_x_offset,
			0,
			0
		])		
		cylinder(
			d2 = Button_diameter + Button_clearance + Surface * 2,
			d1 = Button_diameter + Button_clearance,
			h = Surface
		);
		
		// LCD cutout
		prismoid(
			size1 = [LCD_viewable_width, LCD_viewable_depth],
			size2 = [LCD_viewable_width + Surface * 2, LCD_viewable_depth + Surface * 2],
			h = Surface
		);		
		
		// screw holes
		screws();
	
		// depressions for screw heads for the LCD
		lcd_screw_hole_size = 2.5;
		floating_height = 2;
		height = LCD_height - LCD_display_height;
		for (x = [-LCD_board_width / 2 + LCD_screw_inset, LCD_board_width / 2 - LCD_screw_inset]) {
			for (y = [-LCD_board_depth / 2 + LCD_screw_inset, LCD_board_depth / 2 - LCD_screw_inset]) {
				translate([x, y, 0])
				cylinder(d = 5.5, h = Surface / 2);
			}
		}
	}
	
	// reinforcement ribs with a cutout for the speaker
	render()
	difference() {
		union() {
			for (y = [-inner_depth() / 2, inner_depth() / 2 - 1]) {
				translate([-inner_width() / 2 + Case_radius, y, -1])
				cube([inner_width() - Case_radius * 2, 1, 1]);
			}
			
			for (x = [-inner_width() / 2, inner_width() / 2 - 1]) {
				translate([x, -inner_depth() / 2 + Case_radius, -1])
				cube([1, inner_depth() - Case_radius * 2, 1]);
			}
		}
	
		translate([Speaker_x_offset - Speaker_depth / 2, inner_depth() / 2 - 1, -1])
		cube([Speaker_depth, 1, 1]);
	}
}


module screw() {
	cylinder(d = Screw_shaft_diameter, h = Screw_height, $fn = 20);
	
	translate([0, 0, Screw_height - Screw_head_height])
	cylinder(d2 = Screw_head_diameter, d1 = Screw_shaft_diameter, h = Screw_head_height, $fn = 20);
}

module screws() {
	for (x = [
		-inner_width() / 2 + Case_radius - sin(45) * Case_radius + Screw_inset,
		inner_width() / 2 - Case_radius + sin(45) * Case_radius - Screw_inset
	]) {
		for (y = [
			-inner_depth() / 2 + Case_radius - cos(45) * Case_radius + Screw_inset,
			inner_depth() / 2 - Case_radius + cos(45) * Case_radius - Screw_inset
		]) {
			translate([x, y, -Screw_height + Surface])
			screw();
		}
	}
}

if ($preview) {
	%speaker();
	%lcd();
	%audio_amp();
	%feather();
	%battery();
	%button();
}

case();
//lid();