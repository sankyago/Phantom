use std::sync::Mutex;

pub type NativeCallback = Box<dyn Fn(&mut hecs::World) + Send + Sync>;

static NATIVE_CALLBACKS: Mutex<Vec<NativeCallback>> = Mutex::new(Vec::new());

pub fn run_native_callbacks(world: &mut hecs::World) {
    let mut native_callbacks = NATIVE_CALLBACKS.lock().unwrap();

    for callback in native_callbacks.as_slice() {
        callback(world);
    }

    native_callbacks.clear();
}

pub fn run(callback: NativeCallback) {
    let mut callbacks = NATIVE_CALLBACKS.lock().unwrap();
    callbacks.push(callback);
}
