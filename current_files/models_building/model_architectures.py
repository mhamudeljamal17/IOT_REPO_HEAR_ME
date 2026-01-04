"""
Model Architecture Definitions - Three Models for Comparison
Fixed to handle actual MFCC dimensions
"""

import tensorflow as tf
from tensorflow.keras import layers, Model
from tensorflow.keras.applications import MobileNetV3Small
from tensorflow.keras.regularizers import l2
from tensorflow.keras.optimizers import Adam
from config import LEARNING_RATE, DROPOUT_RATE, L2_REGULARIZATION


class ModelArchitectures:
    """Contains all three model architectures"""

    @staticmethod
    def MiniVGG16(input_shape):
        """
        MiniVGG16 Architecture - Best for accuracy
        Similar to VGG16 but optimized for small feature maps
        """
        model = tf.keras.models.Sequential([
            # Block 1
            layers.Conv2D(32, (3, 3), activation='relu',
                         kernel_regularizer=l2(L2_REGULARIZATION),
                         padding='same', strides=1, input_shape=input_shape),
            layers.MaxPooling2D((3, 3)),

            # Block 2
            layers.Conv2D(16, (3, 3), padding='same', strides=1, activation='relu'),
            layers.Conv2D(16, (3, 3), padding='same', strides=1, activation='relu'),
            layers.MaxPooling2D((3, 3)),

            # Flatten and Dense layers
            layers.Flatten(),

            layers.Dense(32, activation='relu', kernel_regularizer=l2(L2_REGULARIZATION)),
            layers.Dropout(DROPOUT_RATE),
            layers.Dense(32, activation='relu'),
            layers.Dropout(DROPOUT_RATE),
            layers.Dense(1, activation='sigmoid')
        ])

        optimizer = Adam(learning_rate=LEARNING_RATE)
        model.compile(optimizer=optimizer, loss='binary_crossentropy', metrics=['accuracy'])

        return model

    @staticmethod
    def MobileNetV3Small(input_shape):
        """
        MobileNetV3 Small - Best for mobile/edge deployment
        Lightweight and efficient for embedded systems
        """
        inputs = layers.Input(shape=input_shape)

        # Get actual dimensions
        current_height = input_shape[0]  # 20
        current_width = input_shape[1]   # 65

        # Pad to minimum (32, 64) but keep width if larger
        target_height = 32
        target_width = max(64, current_width)

        pad_top = (target_height - current_height) // 2
        pad_bottom = target_height - current_height - pad_top
        pad_left = (target_width - current_width) // 2
        pad_right = target_width - current_width - pad_left

        # Ensure no negative padding
        pad_top = max(0, pad_top)
        pad_bottom = max(0, pad_bottom)
        pad_left = max(0, pad_left)
        pad_right = max(0, pad_right)

        # Zero padding to meet MobileNetV3 minimum input size
        if pad_top > 0 or pad_bottom > 0 or pad_left > 0 or pad_right > 0:
            x = layers.ZeroPadding2D(((pad_top, pad_bottom), (pad_left, pad_right)))(inputs)
        else:
            x = inputs

        # MobileNetV3Small base
        base_model = MobileNetV3Small(
            input_shape=(target_height, target_width, 1),
            include_top=False,
            weights=None,
            include_preprocessing=False,
            pooling=None,
            alpha=0.5,
            dropout_rate=0.1
        )

        # Freeze first 10 layers for stability
        for layer in base_model.layers[:10]:
            layer.trainable = False

        x = base_model(x)

        # Custom layers
        x = layers.GlobalAveragePooling2D()(x)
        x = layers.Dense(32, activation='relu', kernel_regularizer=l2(L2_REGULARIZATION))(x)
        x = layers.Dropout(DROPOUT_RATE)(x)
        x = layers.Dense(32, activation='relu', kernel_regularizer=l2(0.3))(x)
        x = layers.Dropout(DROPOUT_RATE)(x)
        outputs = layers.Dense(1, activation='sigmoid')(x)

        model = Model(inputs, outputs)

        optimizer = Adam(learning_rate=LEARNING_RATE)
        model.compile(optimizer=optimizer, loss='binary_crossentropy', metrics=['accuracy'])

        return model

    @staticmethod
    def SqueezeNet(input_shape):
        """
        SqueezeNet - Best for compression
        Extremely efficient architecture using Fire modules
        """
        def fire_module(x, squeeze_filters, expand_filters):
            # Squeeze layer
            squeeze = layers.Conv2D(squeeze_filters, (1, 1), padding='valid')(x)
            squeeze = layers.BatchNormalization()(squeeze)
            squeeze = layers.Activation('relu')(squeeze)

            # Expand with 1x1
            expand_1x1 = layers.Conv2D(expand_filters, (1, 1), padding='valid')(squeeze)
            expand_1x1 = layers.BatchNormalization()(expand_1x1)
            expand_1x1 = layers.Activation('relu')(expand_1x1)

            # Expand with 3x3
            expand_3x3 = layers.Conv2D(expand_filters, (3, 3), padding='same')(squeeze)
            expand_3x3 = layers.BatchNormalization()(expand_3x3)
            expand_3x3 = layers.Activation('relu')(expand_3x3)

            # Concatenate expansions
            return layers.concatenate([expand_1x1, expand_3x3], axis=-1)

        inputs = layers.Input(shape=input_shape)

        # Get actual dimensions
        current_height = input_shape[0]
        current_width = input_shape[1]

        # Pad to minimum (32, 64) but keep width if larger
        target_height = 32
        target_width = max(64, current_width)

        pad_top = (target_height - current_height) // 2
        pad_bottom = target_height - current_height - pad_top
        pad_left = (target_width - current_width) // 2
        pad_right = target_width - current_width - pad_left

        # Ensure no negative padding
        pad_top = max(0, pad_top)
        pad_bottom = max(0, pad_bottom)
        pad_left = max(0, pad_left)
        pad_right = max(0, pad_right)

        # Padding and channel conversion
        if pad_top > 0 or pad_bottom > 0 or pad_left > 0 or pad_right > 0:
            x = layers.ZeroPadding2D(((pad_top, pad_bottom), (pad_left, pad_right)))(inputs)
        else:
            x = inputs

        x = layers.Conv2D(3, (1, 1), padding='same')(x)

        # Initial convolution
        x = layers.Conv2D(96, (7, 7), strides=(1, 1), padding='same')(x)
        x = layers.BatchNormalization()(x)
        x = layers.Activation('relu')(x)
        x = layers.MaxPooling2D(pool_size=(3, 3), strides=(2, 2), padding='same')(x)

        # Fire modules
        x = fire_module(x, squeeze_filters=16, expand_filters=64)
        x = fire_module(x, squeeze_filters=16, expand_filters=64)
        x = fire_module(x, squeeze_filters=32, expand_filters=128)

        x = layers.MaxPooling2D(pool_size=(3, 3), strides=(2, 2), padding='same')(x)

        x = fire_module(x, squeeze_filters=32, expand_filters=128)
        x = fire_module(x, squeeze_filters=48, expand_filters=192)
        x = fire_module(x, squeeze_filters=48, expand_filters=192)
        x = fire_module(x, squeeze_filters=64, expand_filters=256)

        x = layers.Dropout(0.5)(x)

        # Final layers
        x = layers.Conv2D(1, (1, 1), padding='valid')(x)
        x = layers.BatchNormalization()(x)
        x = layers.Activation('relu')(x)
        x = layers.GlobalAveragePooling2D()(x)

        outputs = layers.Dense(1, activation='sigmoid')(x)

        model = Model(inputs, outputs)

        optimizer = Adam(learning_rate=LEARNING_RATE)
        model.compile(optimizer=optimizer, loss='binary_crossentropy', metrics=['accuracy'])

        return model