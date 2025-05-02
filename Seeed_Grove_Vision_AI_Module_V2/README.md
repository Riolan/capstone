This is a modified implementation for the yolo_v8_od seen from: https://github.com/HimaxWiseEyePlus/Seeed_Grove_Vision_AI_Module_V2?tab=readme-ov-file#build-the-firmware-at-linux-environment
We implement Sliced Aided Hyper Inference (SAHI), and have additionally set up capabilities for FatFS.
We also modfy buffers to will out PSRAM2 previously unoccupied, and camera to not bin keeping it at 640x480px.

Inside here are also files related to the final iterations undertaken for training, quantizing, and vela compiling the YOLOv8n model.

The output.img is one of the final renditions of firmware to be uploaded. 
And 4_28_best_full_integer_quant_vela_non_ops_removed.tflite is the latest trained model.
