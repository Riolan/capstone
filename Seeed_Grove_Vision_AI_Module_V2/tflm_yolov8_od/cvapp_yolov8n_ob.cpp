/*
 * cvapp.cpp
 *
 *  Created on: 2018�~12��4��
 *      Author: 902452
 */

#include <cstdio>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "WE2_device.h"
#include "board.h"
#include "cvapp_yolov8n_ob.h"
#include "cisdp_sensor.h"

#include "WE2_core.h"

#include "ethosu_driver.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/c/common.h"
#if TFLM2209_U55TAG2205
#include "tensorflow/lite/micro/micro_error_reporter.h"
#endif
#include "img_proc_helium.h"
#include "yolo_postprocessing.h"

#include "sahi.hpp"
#include "xprintf.h"
#include "spi_master_protocol.h"
#include "cisdp_cfg.h"
#include "memory_manage.h"
#include <send_result.h>

constexpr int SAHI_SLICE_WIDTH = 192;
constexpr int SAHI_SLICE_HEIGHT = 192;
constexpr int SAHI_SLICE_CHANNELS = 3; // Assuming RGB input needed by model prep stage
constexpr size_t SAHI_SLICE_BUFFER_SIZE = SAHI_SLICE_WIDTH * SAHI_SLICE_HEIGHT * SAHI_SLICE_CHANNELS;
#define SLICE_WIDTH_HEIGHT 192



#define SAHI_CONF_THRESHOLD 0.025f // Confidence threshold for final merge
#define SAHI_IOU_THRESHOLD 0.045f  // IoU threshold for final merge (cross-slice NMS)
#define MODEL_SCORE_THRESHOLD 0.025f // Threshold used *within* slice post-processing
#define MODEL_NMS_THRESHOLD 0.045f   // NMS threshold used *within* slice post-processing



// Declare the buffer and assign it to the custom section mapped to SRAM2
__attribute__((section(".sram2_buffer_data"))) uint8_t g_sahi_slice_buffer[SAHI_SLICE_BUFFER_SIZE] __ALIGNED(32);

#define CHANGE_YOLOV8_OB_OUPUT_SHAPE 0


#define INPUT_IMAGE_CHANNELS 3

#if 1
#define YOLOV8_OB_INPUT_TENSOR_WIDTH   192
#define YOLOV8_OB_INPUT_TENSOR_HEIGHT  192
#define YOLOV8_OB_INPUT_TENSOR_CHANNEL INPUT_IMAGE_CHANNELS
#else
#define YOLOV8_OB_INPUT_TENSOR_WIDTH   224
#define YOLOV8_OB_INPUT_TENSOR_HEIGHT  224
#define YOLOV8_OB_INPUT_TENSOR_CHANNEL INPUT_IMAGE_CHANNELS
#endif

#define YOLOV8N_OB_DBG_APP_LOG 0


// #define EACH_STEP_TICK
#define TOTAL_STEP_TICK
#define YOLOV8_POST_EACH_STEP_TICK 0
uint32_t systick_1, systick_2;
uint32_t loop_cnt_1, loop_cnt_2;
#define CPU_CLK	0xffffff+1
static uint32_t capture_image_tick = 0;
#ifdef TRUSTZONE_SEC
#define U55_BASE	BASE_ADDR_APB_U55_CTRL_ALIAS
#else
#ifndef TRUSTZONE
#define U55_BASE	BASE_ADDR_APB_U55_CTRL_ALIAS
#else
#define U55_BASE	BASE_ADDR_APB_U55_CTRL
#endif
#endif


using namespace std;

namespace {

constexpr int tensor_arena_size = 1020*1024; //1012*1024;// 1036288 | //1053*1024; 

static uint32_t tensor_arena=0;

struct ethosu_driver ethosu_drv; /* Default Ethos-U device driver */
tflite::MicroInterpreter *yolov8n_ob_int_ptr=nullptr;
TfLiteTensor *yolov8n_ob_input, *yolov8n_ob_output, *yolov8n_ob_output2;
};

#if YOLOV8N_OB_DBG_APP_LOG
	std::string coco_classes[] = {"cat", "dog", "squirrel", "bird"};
	int coco_ids[] = {1, 2, 3, 4};

#endif

static void _arm_npu_irq_handler(void)
{
    /* Call the default interrupt handler from the NPU driver */
    ethosu_irq_handler(&ethosu_drv);
}

/**
 * @brief  Initialises the NPU IRQ
 **/
static void _arm_npu_irq_init(void)
{
    const IRQn_Type ethosu_irqnum = (IRQn_Type)U55_IRQn;

    /* Register the EthosU IRQ handler in our vector table.
     * Note, this handler comes from the EthosU driver */
    EPII_NVIC_SetVector(ethosu_irqnum, (uint32_t)_arm_npu_irq_handler);

    /* Enable the IRQ */
    NVIC_EnableIRQ(ethosu_irqnum);

}

static int _arm_npu_init(bool security_enable, bool privilege_enable)
{
    int err = 0;

    /* Initialise the IRQ */
    _arm_npu_irq_init();

    /* Initialise Ethos-U55 device */
#if TFLM2209_U55TAG2205
	const void * ethosu_base_address = (void *)(U55_BASE);
#else 
	void * const ethosu_base_address = (void *)(U55_BASE);
#endif

    if (0 != (err = ethosu_init(
                            &ethosu_drv,             /* Ethos-U driver device pointer */
                            ethosu_base_address,     /* Ethos-U NPU's base address. */
                            NULL,       /* Pointer to fast mem area - NULL for U55. */
                            0, /* Fast mem region size. */
							security_enable,                       /* Security enable. */
							privilege_enable))) {                   /* Privilege enable. */
    	xprintf("failed to initalise Ethos-U device\n");
            return err;
        }

    xprintf("Ethos-U55 device initialised\n");

    return 0;
}


int cv_yolov8n_ob_init(bool security_enable, bool privilege_enable, uint32_t model_addr) {
	int ercode = 0;

	//set memory allocation to tensor_arena
	tensor_arena = mm_reserve_align(tensor_arena_size,0x20); //1mb
	xprintf("TA[%x]\r\n",tensor_arena);


	if(_arm_npu_init(security_enable, privilege_enable)!=0)
		return -1;

	if(model_addr != 0) {
		static const tflite::Model*yolov8n_ob_model = tflite::GetModel((const void *)model_addr);

		if (yolov8n_ob_model->version() != TFLITE_SCHEMA_VERSION) {
			xprintf(
				"[ERROR] yolov8n_ob_model's schema version %d is not equal "
				"to supported version %d\n",
				yolov8n_ob_model->version(), TFLITE_SCHEMA_VERSION);
			return -1;
		}
		else {
			xprintf("yolov8n_ob model's schema version %d\n", yolov8n_ob_model->version());
		}
		#if TFLM2209_U55TAG2205
		static tflite::MicroErrorReporter yolov8n_ob_micro_error_reporter;
		#endif
		static tflite::MicroMutableOpResolver<2> yolov8n_ob_op_resolver;

		yolov8n_ob_op_resolver.AddTranspose();
		if (kTfLiteOk != yolov8n_ob_op_resolver.AddEthosU()){
			xprintf("Failed to add Arm NPU support to op resolver.");
			return false;
		}
		#if TFLM2209_U55TAG2205
			static tflite::MicroInterpreter yolov8n_ob_static_interpreter(yolov8n_ob_model, yolov8n_ob_op_resolver,
					(uint8_t*)tensor_arena, tensor_arena_size, &yolov8n_ob_micro_error_reporter);
		#else
			static tflite::MicroInterpreter yolov8n_ob_static_interpreter(yolov8n_ob_model, yolov8n_ob_op_resolver,
					(uint8_t*)tensor_arena, tensor_arena_size);  
		#endif  


		if(yolov8n_ob_static_interpreter.AllocateTensors()!= kTfLiteOk) {
			return false;
		}

		// ***** CHECKING TENSOR ARENA SIZE *****
		size_t used_bytes = yolov8n_ob_static_interpreter.arena_used_bytes();
		xprintf("Tensor Arena actual used: 0x%x bytes\r\n", used_bytes);
		// *************************


		yolov8n_ob_int_ptr = &yolov8n_ob_static_interpreter;
		yolov8n_ob_input = yolov8n_ob_static_interpreter.input(0);
		yolov8n_ob_output = yolov8n_ob_static_interpreter.output(0);
		//#if CHANGE_YOLOV8_OB_OUPUT_SHAPE
		//	yolov8n_ob_output2 = yolov8n_ob_static_interpreter.output(1);
		//#endif
	}

	xprintf("initial done\n");
	return ercode;
}


#ifndef DETECTION_CLS_YOLOV8
typedef struct detection_cls_yolov8{
    box bbox;
    float confidence;
    float index;

} detection_cls_yolov8;
#define DETECTION_CLS_YOLOV8 1
#endif 



static bool yolov8_det_comparator(detection_cls_yolov8 &pa, detection_cls_yolov8 &pb)
{
    return pa.confidence > pb.confidence;
}

static void  yolov8_NMSBoxes(std::vector<box> &boxes,std::vector<float> &confidences,float modelScoreThreshold,float modelNMSThreshold,std::vector<int>& nms_result)
{
    detection_cls_yolov8 yolov8_bbox;
    std::vector<detection_cls_yolov8> yolov8_bboxes{};
    for(int i = 0; i < boxes.size(); i++)
    {
        yolov8_bbox.bbox = boxes[i];
        yolov8_bbox.confidence = confidences[i];
        yolov8_bbox.index = i;
        yolov8_bboxes.push_back(yolov8_bbox);
    }
    sort(yolov8_bboxes.begin(), yolov8_bboxes.end(), yolov8_det_comparator);
    int updated_size = yolov8_bboxes.size();
    for(int k = 0; k < updated_size; k++)
    {
        if(yolov8_bboxes[k].confidence < modelScoreThreshold)
        {
            continue;
        }
        
        nms_result.push_back(yolov8_bboxes[k].index);
        for(int j = k + 1; j < updated_size; j++)
        {
            float iou = box_iou(yolov8_bboxes[k].bbox, yolov8_bboxes[j].bbox);
            // float iou = box_diou(yolov8_bboxes[k].bbox, yolov8_bboxes[j].bbox);
            if(iou > modelNMSThreshold)
            {
                yolov8_bboxes.erase(yolov8_bboxes.begin() + j);
                updated_size = yolov8_bboxes.size();
                j = j -1;
            }
        }

    }
}


uint32_t copy_mem_to_mem(uint32_t src_addr, uint32_t dst_addr, uint32_t src_w, uint32_t src_h, uint32_t start_x, uint32_t start_y, uint32_t len_x, uint32_t len_y) {
	
	//dbg_printf(DBG_MORE_INFO, "copy_mem_to_mem, src=0x%x, dst=0x%x\n", src_addr, dst_addr);
	//dbg_printf(DBG_MORE_INFO, "src w=%d, h=%d, start_x=%d, y=%d\n", src_w, src_h, start_x, start_y);
	//dbg_printf(DBG_MORE_INFO, "dst w=%d, h=%d\n", len_x, len_y);
	// Fixed for cpp explicit cast
	// uint8_t *cur_src = (uint8_t *)src_addr + start_y * src_w;
	// uint8_t *cur_dst = dst_addr;

	uint8_t *cur_src = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(src_addr)) + start_y * src_w;
	uint8_t *cur_dst = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(dst_addr));
	hx_InvalidateDCache_by_Addr((volatile void *)src_addr, src_w*src_h);

	for(uint32_t j=0;j<len_y;j++)
	{	
		memcpy(cur_dst, cur_src+start_x, len_x);
		cur_src += src_w;
		cur_dst += len_x;		
	}

	hx_CleanDCache_by_Addr((volatile void *)dst_addr, len_x*len_y);
	return 0;
}



// float slice_w;          // The original width of the current slice (e.g., 192)
// float slice_h;          // The original height of the current slice (e.g., 192)
// uint16_t slice_origin_x; // The x-offset of this slice in the large image
// uint16_t slice_origin_y; // The y-offset of this slice in the large image

static void yolov8_ob_post_processing_sahi(tflite::MicroInterpreter* static_interpreter,
	float modelScoreThreshold, float modelNMSThreshold, 
	struct_yolov8_ob_algoResult *alg,	
	std::forward_list<el_box_t> &el_algo,
	float slice_w, float slice_h, uint16_t slice_origin_x, uint16_t slice_origin_y)
{
	//uint32_t img_w = app_get_raw_width();
    //uint32_t img_h = app_get_raw_height();

	TfLiteTensor* output = static_interpreter->output(0);
	//TfLiteTensor* output_2 = static_interpreter->output(1);
	// init postprocessing 	
	//int num_classes = output_2->dims->data[2];
	int num_classes = output->dims->data[1] - 4;
	#if YOLOV8N_OB_DBG_APP_LOG
		//xprintf("output->dims->data[0]: %d\r\n",output->dims->data[0]);//1
		//xprintf("output->dims->data[1]: %d\r\n",output->dims->data[1]);//4
		//xprintf("output->dims->data[2]: %d\r\n",output->dims->data[2]);//756

		//xprintf("output_2->dims->data[0]: %d\r\n",output_2->dims->data[0]);//1
	//	xprintf("output_2->dims->data[1]: %d\r\n",output_2->dims->data[1]);//756
		//xprintf("output_2->dims->data[2]: %d\r\n",output_2->dims->data[2]);//80
	#endif
	// end init
	///////////////////////
	// start postprocessing
	int nboxes=0;
	int input_w = YOLOV8_OB_INPUT_TENSOR_WIDTH;
	int input_h = YOLOV8_OB_INPUT_TENSOR_HEIGHT;

	std::vector<uint16_t> class_idxs;
	std::vector<float> confidences;
	std::vector<box> boxes;


	float output_scale = ((TfLiteAffineQuantization*)(output->quantization.params))->scale->data[0];
	int output_zeropoint = ((TfLiteAffineQuantization*)(output->quantization.params))->zero_point->data[0];
	int output_size = output->bytes;
	//float output_2_scale = ((TfLiteAffineQuantization*)(output_2->quantization.params))->scale->data[0];
	//int output_2_zeropoint = ((TfLiteAffineQuantization*)(output_2->quantization.params))->zero_point->data[0];


	#if YOLOV8N_OB_DBG_APP_LOG
		//printf("output_scale: %f\r\n",output_scale);
		//xprintf("output_zeropoint: %d\r\n",output_zeropoint);

		//printf("output_2_scale: %f\r\n",output_2_scale);
		//xprintf("output_2_zeropoint: %d\r\n",output_2_zeropoint);
	#endif
	/***
	 * dequantize the output result for box
	 * 
	 * 
	 ******/
	/*for(int dims_cnt_2 = 0; dims_cnt_2 < output->dims->data[2]; dims_cnt_2++)
	{
		float outputs_bbox_data[4];
		float maxScore = (-1);// the first four indexes are bbox information
		uint16_t maxClassIndex = 0;
		for(int dims_cnt_1 = 0; dims_cnt_1 < output->dims->data[1]; dims_cnt_1++)// output->dims->data[1] is 4 
		{
			int value =  output->data.int8[ dims_cnt_2 + dims_cnt_1 * output->dims->data[2]];
			
			float deq_value = ((float) value-(float)output_zeropoint) * output_scale ;

			// fix big score
			if(dims_cnt_1%2)//==1
			{
				deq_value *= (float)input_h;
			}
			else
			{
				deq_value *= (float)input_w;
			}
			outputs_bbox_data[dims_cnt_1] = deq_value;
		}

		for(int output_2_dims_cnt_1 = 0; output_2_dims_cnt_1 < output_2->dims->data[2]; output_2_dims_cnt_1++)//output_2->dims->data[2] is 80
		{
			int value_2 =  output_2->data.int8[ output_2_dims_cnt_1 + dims_cnt_2 * output_2->dims->data[2]];
			
			float deq_value_2 = ((float) value_2-(float)output_2_zeropoint) * output_2_scale ;

			if(maxScore < deq_value_2)
			{
				maxScore = deq_value_2;
				maxClassIndex = output_2_dims_cnt_1;
			}
		}
		if (maxScore >= modelScoreThreshold)
		{
			box bbox;
			
			bbox.x = (outputs_bbox_data[0] - (0.5 * outputs_bbox_data[2]));
			bbox.y = (outputs_bbox_data[1] - (0.5 * outputs_bbox_data[3]));
			bbox.w =(outputs_bbox_data[2]);
			bbox.h = (outputs_bbox_data[3]);
			boxes.push_back(bbox);
			class_idxs.push_back(maxClassIndex);
			confidences.push_back(maxScore);
			
		}
	}*/

	for(int dims_cnt_2 = 0; dims_cnt_2 < output->dims->data[2]; dims_cnt_2++)
	{
		float outputs_bbox_data[4];
		float maxScore = (-1);// the first four indexes are bbox information
		uint16_t maxClassIndex = 0;
		for(int dims_cnt_1 = 0; dims_cnt_1 < output->dims->data[1]; dims_cnt_1++)
		{
			int value =  output->data.int8[ dims_cnt_2 + dims_cnt_1 * output->dims->data[2]];
			
			float deq_value = ((float) value-(float)output_zeropoint) * output_scale ;
			if(dims_cnt_1<4)
			{
				/***
				 * fix big score
				 * ****/
				if(dims_cnt_1%2)//==1
				{
					deq_value *= (float)input_h;
				}
				else
				{
					deq_value *= (float)input_w;
				}
				outputs_bbox_data[dims_cnt_1] = deq_value;
			}
			else
			{
				/***
				 * find maximum Score and correspond Class idx
				 * **/
				if(maxScore < deq_value)
				{
					maxScore = deq_value;
					maxClassIndex = dims_cnt_1-4;
				}
			}

		}
		if (maxScore >= modelScoreThreshold)
		{
			box bbox;
			
			bbox.x = (outputs_bbox_data[0] - (0.5 * outputs_bbox_data[2]));
			bbox.y = (outputs_bbox_data[1] - (0.5 * outputs_bbox_data[3]));
			bbox.w =(outputs_bbox_data[2]);
			bbox.h = (outputs_bbox_data[3]);
			boxes.push_back(bbox);
			class_idxs.push_back(maxClassIndex);
			confidences.push_back(maxScore);
			
		}
	}

	
	#if YOLOV8N_OB_DBG_APP_LOG
		xprintf("boxes.size(): %d\r\n",boxes.size());
	#endif
	/**
	 * do nms
	 * 
	 * **/

	std::vector<int> nms_result;
	yolov8_NMSBoxes(boxes, confidences, modelScoreThreshold, modelNMSThreshold, nms_result);
	#if YOLOV8N_OB_DBG_APP_LOG
		xprintf("nms_result.size(): %d\r\n",nms_result.size());
	#endif
	for (int i = 0; i < nms_result.size(); i++)
	{
		if(!(MAX_TRACKED_YOLOV8_ALGO_RES - i)) break;
		int idx = nms_result[i];

		// --- Step 1: Scale coordinates relative to the SLICE dimensions ---
		// Use slice dimensions for scaling, not the original large image dimensions yet
		float scale_factor_w_slice = slice_w / (float)YOLOV8_OB_INPUT_TENSOR_WIDTH;
		float scale_factor_h_slice = slice_h / (float)YOLOV8_OB_INPUT_TENSOR_HEIGHT;

		// Calculate coordinates and dimensions relative to the slice
		float slice_rel_x = boxes[idx].x * scale_factor_w_slice;
		float slice_rel_y = boxes[idx].y * scale_factor_h_slice;
		float slice_rel_w = boxes[idx].w * scale_factor_w_slice;
		float slice_rel_h = boxes[idx].h * scale_factor_h_slice;

		// --- Step 2: Adjust coordinates to GLOBAL image space by adding offset ---
		// Add the slice's origin offset to the top-left coordinates
		uint32_t global_x = (uint32_t)(slice_rel_x + slice_origin_x);
		uint32_t global_y = (uint32_t)(slice_rel_y + slice_origin_y);

		// Width and height remain the scaled values (no offset added)
		uint32_t global_w = (uint32_t)slice_rel_w;
		uint32_t global_h = (uint32_t)slice_rel_h;

		// --- Create the box with GLOBAL coordinates ---
		el_box_t temp_el_box;


		float confidence_0_to_1 = confidences[idx]; // Get original confidence (0.0 to 1.0)

		// Scale to 0.0 - 100.0
		float scaled_score = confidence_0_to_1 * 100.0f;

		// Cast to int (truncates decimal), ensure minimum value is 1
		int score_1_to_100 = std::max(1, static_cast<int>(scaled_score));

		// Clamp the max just in case float calculation slightly exceeds 100.0
		score_1_to_100 = std::min(100, score_1_to_100);

		// Assign to the uint8_t field
		temp_el_box.score = static_cast<uint8_t>(score_1_to_100);
		//temp_el_box.score = (uint8_t)(confidences[idx] * 255); // Scale score 0-1 to 0-255 maybe? Adjust as needed for uint8_t
		temp_el_box.target = class_idxs[idx];

		// Assign GLOBAL coordinates and dimensions
		// Add checks for potential overflow if global_x/y/w/h might exceed uint16_t limits!
		if (global_x > UINT16_MAX || global_y > UINT16_MAX || global_w > UINT16_MAX || global_h > UINT16_MAX) {
			// Handle overflow: clamp, skip, log error, etc.
			printf("Warning: Box coordinates/dimensions exceed uint16_t limits after offset/scaling.\n");
			continue; // Example: skip this box
		}
		temp_el_box.x = (uint16_t)global_x;
		temp_el_box.y = (uint16_t)global_y;
		temp_el_box.w = (uint16_t)global_w;
		temp_el_box.h = (uint16_t)global_h;


		// printf("global_box.x %d, global_box.y: %d\r\n", temp_el_box.x, temp_el_box.y);
		el_algo.emplace_front(temp_el_box); // Add the globally adjusted box

		#if YOLOV8N_OB_DBG_APP_LOG
			printf("detect object[%d]: %s confidences: %f at global (%d, %d)\r\n", i, coco_classes[class_idxs[idx]].c_str(), confidences[idx], temp_el_box.x, temp_el_box.y);
		#endif
	}
}


static void yolov8_ob_post_processing(tflite::MicroInterpreter* static_interpreter,float modelScoreThreshold, float modelNMSThreshold, struct_yolov8_ob_algoResult *alg, std::forward_list<el_box_t> &el_algo)
{
	uint32_t img_w = app_get_raw_width();
    uint32_t img_h = app_get_raw_height();
	TfLiteTensor* output = static_interpreter->output(0);
	// init postprocessing 	
	int num_classes = output->dims->data[1] - 4;

	
	// end init
	///////////////////////
	// start postprocessing
	int nboxes=0;
	int input_w = YOLOV8_OB_INPUT_TENSOR_WIDTH;
	int input_h = YOLOV8_OB_INPUT_TENSOR_HEIGHT;

	std::vector<uint16_t> class_idxs;
	std::vector<float> confidences;
	std::vector<box> boxes;


	float output_scale = ((TfLiteAffineQuantization*)(output->quantization.params))->scale->data[0];
	int output_zeropoint = ((TfLiteAffineQuantization*)(output->quantization.params))->zero_point->data[0];
	int output_size = output->bytes;

	#if YOLOV8N_OB_DBG_APP_LOG
		// xprintf("output->dims->size: %d\r\n",output->dims->size);
		// printf("output_scale: %f\r\n",output_scale);
		// xprintf("output_zeropoint: %d\r\n",output_zeropoint);
		// xprintf("output_size: %d\r\n",output_size);
		// xprintf("output->dims->data[0]: %d\r\n",output->dims->data[0]);//1
		// xprintf("output->dims->data[1]: %d\r\n",output->dims->data[1]);//84
		// xprintf("output->dims->data[2]: %d\r\n",output->dims->data[2]);//756
	#endif
	/***
	 * dequantize the output result
	 * 
	 * 
	 ******/
	for(int dims_cnt_2 = 0; dims_cnt_2 < output->dims->data[2]; dims_cnt_2++)
	{
		float outputs_bbox_data[4];
		float maxScore = (-1);// the first four indexes are bbox information
		uint16_t maxClassIndex = 0;
		for(int dims_cnt_1 = 0; dims_cnt_1 < output->dims->data[1]; dims_cnt_1++)
		{
			int value =  output->data.int8[ dims_cnt_2 + dims_cnt_1 * output->dims->data[2]];
			
			float deq_value = ((float) value-(float)output_zeropoint) * output_scale ;
			if(dims_cnt_1<4)
			{
				/***
				 * fix big score
				 * ****/
				if(dims_cnt_1%2)//==1
				{
					deq_value *= (float)input_h;
				}
				else
				{
					deq_value *= (float)input_w;
				}
				outputs_bbox_data[dims_cnt_1] = deq_value;
			}
			else
			{
				/***
				 * find maximum Score and correspond Class idx
				 * **/
				if(maxScore < deq_value)
				{
					maxScore = deq_value;
					maxClassIndex = dims_cnt_1-4;
				}
			}

		}
		if (maxScore >= modelScoreThreshold)
		{
			box bbox;
			
			bbox.x = (outputs_bbox_data[0] - (0.5 * outputs_bbox_data[2]));
			bbox.y = (outputs_bbox_data[1] - (0.5 * outputs_bbox_data[3]));
			bbox.w =(outputs_bbox_data[2]);
			bbox.h = (outputs_bbox_data[3]);
			boxes.push_back(bbox);
			class_idxs.push_back(maxClassIndex);
			confidences.push_back(maxScore);
			
		}
	}
	#if YOLOV8N_OB_DBG_APP_LOG
		xprintf("boxes.size(): %d\r\n",boxes.size());
	#endif
	/**
	 * do nms
	 * 
	 * **/

	std::vector<int> nms_result;
	yolov8_NMSBoxes(boxes, confidences, modelScoreThreshold, modelNMSThreshold, nms_result);
	for (int i = 0; i < nms_result.size(); i++)
	{
		if(!(MAX_TRACKED_YOLOV8_ALGO_RES-i))break;
		int idx = nms_result[i];

		float scale_factor_w = (float)img_w / (float)YOLOV8_OB_INPUT_TENSOR_WIDTH; 
		float scale_factor_h = (float)img_h / (float)YOLOV8_OB_INPUT_TENSOR_HEIGHT; 
		alg->obr[i].confidence = confidences[idx];
		alg->obr[i].bbox.x = (uint32_t)(boxes[idx].x * scale_factor_w);
		alg->obr[i].bbox.y = (uint32_t)(boxes[idx].y * scale_factor_h);
		alg->obr[i].bbox.width = (uint32_t)(boxes[idx].w * scale_factor_w);
		alg->obr[i].bbox.height = (uint32_t)(boxes[idx].h * scale_factor_h);
		alg->obr[i].class_idx = class_idxs[idx];

		el_box_t temp_el_box;
		temp_el_box.score =  confidences[idx]*100;
		temp_el_box.target =  class_idxs[idx];
		temp_el_box.x = (uint32_t)(boxes[idx].x * scale_factor_w);
		temp_el_box.y =  (uint32_t)(boxes[idx].y * scale_factor_h);
		temp_el_box.w = (uint32_t)(boxes[idx].w * scale_factor_w);
		temp_el_box.h = (uint32_t)(boxes[idx].h * scale_factor_h);


		// printf("temp_el_box.x %d,temp_el_box.y: %d\r\n",temp_el_box.x,temp_el_box.y);
		el_algo.emplace_front(temp_el_box);
		#if YOLOV8N_OB_DBG_APP_LOG
			printf("detect object[%d]: %s confidences: %f\r\n",i, coco_classes[class_idxs[idx]].c_str(),confidences[idx]);

		#endif
	}
}

/*
int cv_yolov8n_ob_run(struct_yolov8_ob_algoResult *algoresult_yolov8n_ob) {
	int ercode = 0;
    float w_scale;
    float h_scale;
    uint32_t img_w = app_get_raw_width();
    uint32_t img_h = app_get_raw_height();
    uint32_t ch = app_get_raw_channels();
    uint32_t raw_addr = app_get_raw_addr();
    uint32_t expand = 0;
	std::forward_list<el_box_t> el_algo;

	#if YOLOV8N_OB_DBG_APP_LOG
    xprintf("raw info: w[%d] h[%d] ch[%d] addr[%x]\n",img_w, img_h, ch, raw_addr);
	#endif

    if(yolov8n_ob_int_ptr!= nullptr) {
		#ifdef TOTAL_STEP_TICK
			SystemGetTick(&systick_1, &loop_cnt_1);
		#endif
		#ifdef EACH_STEP_TICK
			SystemGetTick(&systick_1, &loop_cnt_1);
		#endif
    	//get image from sensor and resize
		w_scale = (float)(img_w - 1) / (YOLOV8_OB_INPUT_TENSOR_WIDTH - 1);
		h_scale = (float)(img_h - 1) / (YOLOV8_OB_INPUT_TENSOR_HEIGHT - 1);

		
		hx_lib_image_resize_BGR8U3C_to_RGB24_helium((uint8_t*)raw_addr, (uint8_t*)yolov8n_ob_input->data.data,  
		                    img_w, img_h, ch, 
                        	YOLOV8_OB_INPUT_TENSOR_WIDTH, YOLOV8_OB_INPUT_TENSOR_HEIGHT, w_scale,h_scale);
		#ifdef EACH_STEP_TICK						
			SystemGetTick(&systick_2, &loop_cnt_2);
			dbg_printf(DBG_LESS_INFO,"Tick for resize image BGR8U3C_to_RGB24_helium for yolov8 OB:[%d]\r\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));							
		#endif

		#ifdef EACH_STEP_TICK
			SystemGetTick(&systick_1, &loop_cnt_1);
		#endif

		// //uint8 to int8
		for (int i = 0; i < yolov8n_ob_input->bytes; ++i) {
			*((int8_t *)yolov8n_ob_input->data.data+i) = *((int8_t *)yolov8n_ob_input->data.data+i) - 128;
    	}

		#ifdef EACH_STEP_TICK
		SystemGetTick(&systick_2, &loop_cnt_2);
		dbg_printf(DBG_LESS_INFO,"Tick for Invoke for uint8toint8 for YOLOV8_OB:[%d]\r\n\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));    
		#endif	

		#ifdef EACH_STEP_TICK
		SystemGetTick(&systick_1, &loop_cnt_1);
		#endif
		TfLiteStatus invoke_status = yolov8n_ob_int_ptr->Invoke();

		#ifdef EACH_STEP_TICK
		SystemGetTick(&systick_2, &loop_cnt_2);
		#endif
		if(invoke_status != kTfLiteOk)
		{
			xprintf("yolov8 object detect invoke fail\n");
			return -1;
		}
		else
		{
			#if YOLOV8N_OB_DBG_APP_LOG
			xprintf("yolov8 object detect  invoke pass\n");
			#endif
		}
		#ifdef EACH_STEP_TICK
    		dbg_printf(DBG_LESS_INFO,"Tick for Invoke for YOLOV8_OB:[%d]\r\n\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));    
		#endif

		#ifdef EACH_STEP_TICK
			SystemGetTick(&systick_1, &loop_cnt_1);
		#endif
		//retrieve output data
		yolov8_ob_post_processing(yolov8n_ob_int_ptr,0.25, 0.45, algoresult_yolov8n_ob, el_algo);
		#ifdef EACH_STEP_TICK
			SystemGetTick(&systick_2, &loop_cnt_2);
			dbg_printf(DBG_LESS_INFO,"Tick for Invoke for YOLOV8_OB_post_processing:[%d]\r\n\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));    
		#endif
		#if YOLOV8N_OB_DBG_APP_LOG
			xprintf("yolov8_ob_post_processing done\r\n");
		#endif
		#ifdef TOTAL_STEP_TICK						
			SystemGetTick(&systick_2, &loop_cnt_2);
			// dbg_printf(DBG_LESS_INFO,"Tick for TOTAL YOLOV8 OB:[%d]\r\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));		
		#endif

    }
	//return 0;

#ifdef UART_SEND_ALOGO_RESEULT
	algoresult_yolov8n_ob->algo_tick = (loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2) + capture_image_tick;
uint32_t judge_case_data;
uint32_t g_trans_type;
hx_drv_swreg_aon_get_appused1(&judge_case_data);
g_trans_type = (judge_case_data>>16);
if( g_trans_type == 0 || g_trans_type == 2)// transfer type is (UART) or (UART & SPI) 
{
	//invalid dcache to let uart can send the right jpeg img out
	hx_InvalidateDCache_by_Addr((volatile void *)app_get_jpeg_addr(), sizeof(uint8_t) *app_get_jpeg_sz());

	el_img_t temp_el_jpg_img = el_img_t{};
	temp_el_jpg_img.data = (uint8_t *)app_get_jpeg_addr();
	temp_el_jpg_img.size = app_get_jpeg_sz();
	temp_el_jpg_img.width = app_get_raw_width();
	temp_el_jpg_img.height = app_get_raw_height();
	temp_el_jpg_img.format = EL_PIXEL_FORMAT_JPEG;
	temp_el_jpg_img.rotate = EL_PIXEL_ROTATE_0;

	send_device_id();
	// event_reply(concat_strings(", ", box_results_2_json_str(el_algo), ", ", img_2_json_str(&temp_el_jpg_img)));
	event_reply(concat_strings(", ", algo_tick_2_json_str(algoresult_yolov8n_ob->algo_tick),", ", box_results_2_json_str(el_algo), ", ", img_2_json_str(&temp_el_jpg_img)));
}
	set_model_change_by_uart();
#endif	

	SystemGetTick(&systick_1, &loop_cnt_1);
	//recapture image
	sensordplib_retrigger_capture();

	
	SystemGetTick(&systick_2, &loop_cnt_2);
	capture_image_tick = (loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2);	
	return ercode;
}
*/
void initialize_random_seed() {
    srand(17389604); 
}

int generate_simple_uuid_v4(char* buffer, size_t buffer_size) {
    if (buffer_size < 37) {
        return -1; // Buffer too small
    }

    unsigned char random_bytes[16];

    for (int i = 0; i < 16; ++i) {
        random_bytes[i] = (unsigned char)(rand() & 0xFF);
    }
    random_bytes[6] = (random_bytes[6] & 0x0F) | 0x40;
    random_bytes[8] = (random_bytes[8] & 0x3F) | 0x80;

    // Format into string
    int written = snprintf(buffer, buffer_size,
                           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                           random_bytes[0], random_bytes[1], random_bytes[2], random_bytes[3],
                           random_bytes[4], random_bytes[5],
                           random_bytes[6], random_bytes[7], 
                           random_bytes[8], random_bytes[9], 
                           random_bytes[10], random_bytes[11], random_bytes[12], random_bytes[13], random_bytes[14], random_bytes[15]);

    if (written != 36) {
        buffer[0] = '\0'; // Ensure empty on error
        return -1;
    }

    return 0; // Success
}


int cv_yolov8n_ob_run(struct_yolov8_ob_algoResult *algoresult_yolov8n_ob) {
	int ercode = 0;
    float w_scale;
    float h_scale;
    uint32_t img_w = app_get_raw_width();
    uint32_t img_h = app_get_raw_height();
    uint32_t ch = app_get_raw_channels();
    uint32_t raw_addr = app_get_raw_addr();
	std::forward_list<el_box_t> el_algo;

	#if YOLOV8N_OB_DBG_APP_LOG
    xprintf("raw info: w[%d] h[%d] ch[%d] addr[%x]\n",img_w, img_h, ch, raw_addr);
	#endif


    if(yolov8n_ob_int_ptr!= nullptr) {
		#ifdef TOTAL_STEP_TICK
			SystemGetTick(&systick_1, &loop_cnt_1);
		#endif
		#ifdef EACH_STEP_TICK
			SystemGetTick(&systick_1, &loop_cnt_1);
		#endif
    	//get image from sensor and resize

		
		auto slices = SAHI::create_slices(
			img_w, img_h, 
			SLICE_WIDTH_HEIGHT, SLICE_WIDTH_HEIGHT, 
			0.2
		);

		for (const auto& slice : slices) {

			//dbg_printf(DBG_LESS_INFO,"Sahi slice x:%d y:%d w:%d h:%d\r\n",slice.x,slice.y,slice.width,slice.height);    


			// Replaced above with: cisdp_get_raw_buff_320_320()
			// Extract slice from raw full image into BGR buffer
			copy_mem_to_mem(
				raw_addr, // this is the raw image buffer
				reinterpret_cast<uint32_t>(g_sahi_slice_buffer), // this is bgr buffer
				SLICE_WIDTH_HEIGHT, SLICE_WIDTH_HEIGHT,
				slice.x, slice.y,
				slice.width, slice.height
			);

			// Convert BGR to RGB
			hx_lib_image_resize_BGR8U3C_to_RGB24_helium(
				g_sahi_slice_buffer, // this is bgr buffer
			 	(uint8_t*)yolov8n_ob_input->data.data, // this is rgb buffer
				slice.width, slice.height, 3,
				SLICE_WIDTH_HEIGHT, SLICE_WIDTH_HEIGHT,
				(float)(slice.width - 1) / (SLICE_WIDTH_HEIGHT - 1),  // these should be 1.0
				(float)(slice.height - 1) / (SLICE_WIDTH_HEIGHT - 1) // these should be 1.0
			);

			#ifdef EACH_STEP_TICK						
				SystemGetTick(&systick_2, &loop_cnt_2);
				dbg_printf(DBG_LESS_INFO,"Tick for resize image BGR8U3C_to_RGB24_helium for yolo OB:[%d]\r\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));							
			#endif


			#ifdef EACH_STEP_TICK
				SystemGetTick(&systick_1, &loop_cnt_1);
			#endif

			// //uint8 to int8
			for (int i = 0; i < yolov8n_ob_input->bytes; ++i) {
				*((int8_t *)yolov8n_ob_input->data.data+i) = *((int8_t *)yolov8n_ob_input->data.data+i) - 128;
			}

			#ifdef EACH_STEP_TICK
			SystemGetTick(&systick_2, &loop_cnt_2);
			dbg_printf(DBG_LESS_INFO,"Tick for Invoke for uint8toint8 for YOLOV8_OB:[%d]\r\n\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));    
			#endif	

			#ifdef EACH_STEP_TICK
			SystemGetTick(&systick_1, &loop_cnt_1);
			#endif
			TfLiteStatus invoke_status = yolov8n_ob_int_ptr->Invoke();

			#ifdef EACH_STEP_TICK
			SystemGetTick(&systick_2, &loop_cnt_2);
			#endif
			if(invoke_status != kTfLiteOk)
			{
				xprintf("yolov8 object detect invoke fail\n");
				return -1;
			}
			else
			{
				#if YOLOV8N_OB_DBG_APP_LOG
				xprintf("yolov8 object detect  invoke pass\n");
				#endif
			}
			#ifdef EACH_STEP_TICK
				dbg_printf(DBG_LESS_INFO,"Tick for Invoke for YOLOV8_OB:[%d]\r\n\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));    
			#endif

			#ifdef EACH_STEP_TICK
				SystemGetTick(&systick_1, &loop_cnt_1);
			#endif
			//retrieve output data

			//struct_yolov8_ob_algoResult slice_algo_result; // NOT el_algo

			//yolov8_ob_post_processing(yolov8n_ob_int_ptr, 0.25, 0.45, algoresult_yolov8n_ob,el_algo);


			//////////
			//static void yolov8_ob_post_processing_sahi(tflite::MicroInterpreter* static_interpreter,
			///	float modelScoreThreshold, float modelNMSThreshold, 
			///	struct_yolov8_ob_algoResult *alg,	
			///	std::forward_list<el_box_t> &el_algo,
			///	float slice_w, float slice_h, uint16_t slice_origin_x, uint16_t slice_origin_y)
			/////////////

			yolov8_ob_post_processing_sahi(
				yolov8n_ob_int_ptr,
				0.03,
				0.05,
				algoresult_yolov8n_ob,
				el_algo,
				192,    // Added for SAHI (actual pixel width of the slice)
				192,
				(uint16_t )slice.x, // Added for SAHI
				(uint16_t )slice.y				
			);

			//return 0; // Added to exit after processing the first slice
			#ifdef EACH_STEP_TICK
				SystemGetTick(&systick_2, &loop_cnt_2);
				dbg_printf(DBG_LESS_INFO,"Tick for Invoke for YOLOV8_OB_post_processing:[%d]\r\n\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));    
			#endif
			#if YOLOV8N_OB_DBG_APP_LOG
				xprintf("yolov8_ob_post_processing done\r\n");
			#endif
			#ifdef TOTAL_STEP_TICK						
				SystemGetTick(&systick_2, &loop_cnt_2);
				// dbg_printf(DBG_LESS_INFO,"Tick for TOTAL YOLOV8 OB:[%d]\r\n",(loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2));		
			#endif
			}  /// END OF FOR LOOP FOR EACH SLICE
	} /// END OF IF YOLOV8N_OB_INT_PTR NOT NULL

	static_assert(std::is_same_v<decltype(el_algo), std::forward_list<el_box_t>>, "el_algo type mismatch");
	//static_assert(std::is_same_v<decltype(nms_iou_thresh), float>, "nms_iou_thresh type mismatch");
	
	float nms_iou_thresh = 0.04;
	auto final_results = SAHI::non_maximum_suppression(el_algo, nms_iou_thresh);

	// Now 'final_results' is a forward_list containing the unique detections
	//printf("NMS finished. Kept boxes: \n");
	//for (const auto& box : final_results) {
	//	printf("  Box: x=%u, y=%u, w=%u, h=%u, score=%u, target=%u\n",
	//			box.x, box.y, box.w, box.h, box.score, box.target);
	//}


	#ifdef UART_SEND_ALOGO_RESEULT
	//xprintf("UART_SEND_ALOGO_RESEULT\r\n");

	algoresult_yolov8n_ob->algo_tick = (loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2) + capture_image_tick;
	uint32_t judge_case_data;
	uint32_t g_trans_type;
	hx_drv_swreg_aon_get_appused1(&judge_case_data);
	g_trans_type = (judge_case_data>>16);
	if( g_trans_type == 0 || g_trans_type == 2)// transfer type is (UART) or (UART & SPI) 
	{
		//invalid dcache to let uart can send the right jpeg img out
		hx_InvalidateDCache_by_Addr((volatile void *)app_get_jpeg_addr(), sizeof(uint8_t) *app_get_jpeg_sz());

		el_img_t temp_el_jpg_img = el_img_t{};
		temp_el_jpg_img.data = (uint8_t *)app_get_jpeg_addr();
		temp_el_jpg_img.size = app_get_jpeg_sz();
		temp_el_jpg_img.width = app_get_raw_width();
		temp_el_jpg_img.height = app_get_raw_height();
		temp_el_jpg_img.format = EL_PIXEL_FORMAT_JPEG;
		temp_el_jpg_img.rotate = EL_PIXEL_ROTATE_0;

		send_device_id();

		char identifier_string[37]; // Use 37 for UUID v4, adjust size if using Device ID format
		int result = -1;
		// Generate a random UUID v4 (changes each time)
		// result = generate_simple_uuid_v4(identifier_string, sizeof(identifier_string));
		result = generate_simple_uuid_v4(identifier_string, sizeof(identifier_string));
		//printf("----------------- \n");
		//printf("GENERATED UUID OF: \n");
		//printf("GENERATED UUID OF: \n");
		//printf("GENERATED UUID OF: \n");

		//printf("File UUID initialized to: %s\r\n", identifier_string);

		//printf("GENERATED UUID OF: \n");
		//printf("GENERATED UUID OF: \n");
		//printf("GENERATED UUID OF: \n");
		//printf("------------------ \n");

		if (result != 0) {
			// Handle error - couldn't generate ID
			strcpy(identifier_string, "error_id"); // Or some default
		}

		// --- Logging example ---
		// char log_uuid_str[37];
		// generate_simple_uuid_v4(log_uuid_str, sizeof(log_uuid_str)); // Ignoring error check for brevity
		// log_printf("[%s] Sensor reading: %d\r\n", log_uuid_str, sensor_value);

		// --- Filenaming example (if using FatFs) ---
		// char file_uuid_str[37];
		// generate_simple_uuid_v4(file_uuid_str, sizeof(file_uuid_str));
		// char filename[64];
		// snprintf(filename, sizeof(filename), "LOG_%s.TXT", file_uuid_str);
		// // ... use FatFs f_open() with filename ...


		// event_reply(concat_strings(", ", box_results_2_json_str(el_algo), ", ", img_2_json_str(&temp_el_jpg_img)));
		// TODO: try just box results.
		event_reply(concat_strings(", ", algo_tick_2_json_str(algoresult_yolov8n_ob->algo_tick),", ", box_results_2_json_str(el_algo), ", ", img_2_json_str(&temp_el_jpg_img)));
	}
	set_model_change_by_uart();
#endif	

	SystemGetTick(&systick_1, &loop_cnt_1);
	//recapture image
	sensordplib_retrigger_capture();

	
	SystemGetTick(&systick_2, &loop_cnt_2);
	capture_image_tick = (loop_cnt_2-loop_cnt_1)*CPU_CLK+(systick_1-systick_2);	

	return ercode;
}

int cv_yolov8n_ob_deinit()
{
	
	return 0;
}

