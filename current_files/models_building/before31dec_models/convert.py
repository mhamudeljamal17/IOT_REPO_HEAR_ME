import tensorflow as tf
import os

# Define your model folders and filenames
models = [
    {
        'folder': r'.\MiniVGG16',
        'filename': 'MiniVGG16_final'
    },
    {
        'folder': r'.\MobileNetV3Small',
        'filename': 'MobileNetV3Small_final'
    },
    {
        'folder': r'.\SqueezeNet',
        'filename': 'SqueezeNet_final'
    }
]

for model_config in models:
    folder = model_config['folder']
    filename = model_config['filename']

    print(f"\n{'=' * 50}")
    print(f"Processing: {filename}")
    print(f"{'=' * 50}")

    # Try .keras first, then .h5
    keras_path = os.path.join(folder, f'{filename}.keras')
    h5_path = os.path.join(folder, f'{filename}.h5')

    model_path = None
    if os.path.exists(keras_path):
        model_path = keras_path
        print(f"Found .keras file: {keras_path}")
    elif os.path.exists(h5_path):
        model_path = h5_path
        print(f"Found .h5 file: {h5_path}")
    else:
        print(f"ERROR: No model file found for {filename}")
        continue

    try:
        # Load model
        print("Loading model...")
        model = tf.keras.models.load_model(model_path)
        print("Model loaded successfully")

        # Convert to TFLite
        print("Converting to TFLite...")
        converter = tf.lite.TFLiteConverter.from_keras_model(model)
        tflite_model = converter.convert()

        # Save TFLite
        tflite_path = os.path.join(folder, f'{filename}.tflite')
        with open(tflite_path, 'wb') as f:
            f.write(tflite_model)
        print(f"TFLite saved: {tflite_path}")

        # Convert TFLite to C++ header
        print("Converting to C++ header...")
        cc_content = f'const unsigned char {filename.replace("-", "_")}_tflite[] = {{\n'

        # Convert binary to hex array
        hex_bytes = ', '.join([f'0x{byte:02x}' for byte in tflite_model])
        cc_content += hex_bytes + '\n};\n'
        cc_content += f'const int {filename.replace("-", "_")}_tflite_len = {len(tflite_model)};\n'

        # Save header file
        header_path = os.path.join(folder, f'{filename}_data.h')
        with open(header_path, 'w') as f:
            f.write(cc_content)
        print(f"Header file saved: {header_path}")

        # Also save as .cc file
        cc_path = os.path.join(folder, f'{filename}.cc')
        with open(cc_path, 'w') as f:
            f.write(cc_content)
        print(f"CC file saved: {cc_path}")

        print(f"✓ {filename} conversion complete!")

    except Exception as e:
        print(f"ERROR processing {filename}: {str(e)}")

print(f"\n{'=' * 50}")
print("All conversions completed!")
print(f"{'=' * 50}")



