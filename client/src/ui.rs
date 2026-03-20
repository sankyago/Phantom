use crate::wrapped_natives::*;

#[derive(Clone, Debug, PartialEq)]
pub struct TextEntry {
    text: String,
    pos_x: f32,
    pos_y: f32,
    scale: f32,
    color: (i32, i32, i32, i32),
}

pub fn ui_startup_system(world: &mut hecs::World) {
    world.spawn((TextEntry {
        text: "Phantom".to_owned(),
        pos_x: 0.96,
        pos_y: 0.01,
        scale: 0.42,
        color: (200, 200, 200, 255),
    },));
}

pub fn draw_text_entries(world: &mut hecs::World) {
    for (_, entry) in world.query::<&TextEntry>().iter() {
        ui::draw_text(
            &entry.text,
            entry.pos_x,
            entry.pos_y,
            entry.scale,
            entry.color,
            true,
            1.0,
        );
    }
}
