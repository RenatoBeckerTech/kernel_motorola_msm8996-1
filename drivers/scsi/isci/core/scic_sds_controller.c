/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/device.h>
#include "scic_controller.h"
#include "scic_phy.h"
#include "scic_port.h"
#include "scic_sds_controller.h"
#include "scu_registers.h"
#include "scic_sds_phy.h"
#include "scic_sds_port_configuration_agent.h"
#include "scic_sds_port.h"
#include "remote_device.h"
#include "scic_sds_request.h"
#include "sci_environment.h"
#include "sci_util.h"
#include "scu_completion_codes.h"
#include "scu_constants.h"
#include "scu_event_codes.h"
#include "scu_remote_node_context.h"
#include "scu_task_context.h"
#include "scu_unsolicited_frame.h"

#define SCU_CONTEXT_RAM_INIT_STALL_TIME      200

/**
 * smu_dcc_get_max_ports() -
 *
 * This macro returns the maximum number of logical ports supported by the
 * hardware. The caller passes in the value read from the device context
 * capacity register and this macro will mash and shift the value appropriately.
 */
#define smu_dcc_get_max_ports(dcc_value) \
	(\
		(((dcc_value) & SMU_DEVICE_CONTEXT_CAPACITY_MAX_LP_MASK) \
		 >> SMU_DEVICE_CONTEXT_CAPACITY_MAX_LP_SHIFT) + 1 \
	)

/**
 * smu_dcc_get_max_task_context() -
 *
 * This macro returns the maximum number of task contexts supported by the
 * hardware. The caller passes in the value read from the device context
 * capacity register and this macro will mash and shift the value appropriately.
 */
#define smu_dcc_get_max_task_context(dcc_value)	\
	(\
		(((dcc_value) & SMU_DEVICE_CONTEXT_CAPACITY_MAX_TC_MASK) \
		 >> SMU_DEVICE_CONTEXT_CAPACITY_MAX_TC_SHIFT) + 1 \
	)

/**
 * smu_dcc_get_max_remote_node_context() -
 *
 * This macro returns the maximum number of remote node contexts supported by
 * the hardware. The caller passes in the value read from the device context
 * capacity register and this macro will mash and shift the value appropriately.
 */
#define smu_dcc_get_max_remote_node_context(dcc_value) \
	(\
		(((dcc_value) & SMU_DEVICE_CONTEXT_CAPACITY_MAX_RNC_MASK) \
		 >> SMU_DEVICE_CONTEXT_CAPACITY_MAX_RNC_SHIFT) + 1 \
	)


static void scic_sds_controller_power_control_timer_handler(
	void *controller);
#define SCIC_SDS_CONTROLLER_MIN_TIMER_COUNT  3
#define SCIC_SDS_CONTROLLER_MAX_TIMER_COUNT  3

/**
 *
 *
 * The number of milliseconds to wait for a phy to start.
 */
#define SCIC_SDS_CONTROLLER_PHY_START_TIMEOUT      100

/**
 *
 *
 * The number of milliseconds to wait while a given phy is consuming power
 * before allowing another set of phys to consume power. Ultimately, this will
 * be specified by OEM parameter.
 */
#define SCIC_SDS_CONTROLLER_POWER_CONTROL_INTERVAL 500

/**
 * COMPLETION_QUEUE_CYCLE_BIT() -
 *
 * This macro will return the cycle bit of the completion queue entry
 */
#define COMPLETION_QUEUE_CYCLE_BIT(x) ((x) & 0x80000000)

/**
 * NORMALIZE_GET_POINTER() -
 *
 * This macro will normalize the completion queue get pointer so its value can
 * be used as an index into an array
 */
#define NORMALIZE_GET_POINTER(x) \
	((x) & SMU_COMPLETION_QUEUE_GET_POINTER_MASK)

/**
 * NORMALIZE_PUT_POINTER() -
 *
 * This macro will normalize the completion queue put pointer so its value can
 * be used as an array inde
 */
#define NORMALIZE_PUT_POINTER(x) \
	((x) & SMU_COMPLETION_QUEUE_PUT_POINTER_MASK)


/**
 * NORMALIZE_GET_POINTER_CYCLE_BIT() -
 *
 * This macro will normalize the completion queue cycle pointer so it matches
 * the completion queue cycle bit
 */
#define NORMALIZE_GET_POINTER_CYCLE_BIT(x) \
	((SMU_CQGR_CYCLE_BIT & (x)) << (31 - SMU_COMPLETION_QUEUE_GET_CYCLE_BIT_SHIFT))

/**
 * NORMALIZE_EVENT_POINTER() -
 *
 * This macro will normalize the completion queue event entry so its value can
 * be used as an index.
 */
#define NORMALIZE_EVENT_POINTER(x) \
	(\
		((x) & SMU_COMPLETION_QUEUE_GET_EVENT_POINTER_MASK) \
		>> SMU_COMPLETION_QUEUE_GET_EVENT_POINTER_SHIFT	\
	)

/**
 * INCREMENT_COMPLETION_QUEUE_GET() -
 *
 * This macro will increment the controllers completion queue index value and
 * possibly toggle the cycle bit if the completion queue index wraps back to 0.
 */
#define INCREMENT_COMPLETION_QUEUE_GET(controller, index, cycle) \
	INCREMENT_QUEUE_GET(\
		(index), \
		(cycle), \
		(controller)->completion_queue_entries,	\
		SMU_CQGR_CYCLE_BIT \
		)

/**
 * INCREMENT_EVENT_QUEUE_GET() -
 *
 * This macro will increment the controllers event queue index value and
 * possibly toggle the event cycle bit if the event queue index wraps back to 0.
 */
#define INCREMENT_EVENT_QUEUE_GET(controller, index, cycle) \
	INCREMENT_QUEUE_GET(\
		(index), \
		(cycle), \
		(controller)->completion_event_entries,	\
		SMU_CQGR_EVENT_CYCLE_BIT \
		)

static void scic_sds_controller_initialize_power_control(struct scic_sds_controller *scic)
{
	struct isci_host *ihost = sci_object_get_association(scic);
	scic->power_control.timer = isci_timer_create(ihost,
						      scic,
					scic_sds_controller_power_control_timer_handler);

	memset(scic->power_control.requesters, 0,
	       sizeof(scic->power_control.requesters));

	scic->power_control.phys_waiting = 0;
	scic->power_control.phys_granted_power = 0;
}

int scic_controller_mem_init(struct scic_sds_controller *scic)
{
	struct device *dev = scic_to_dev(scic);
	dma_addr_t dma_handle;
	enum sci_status result;

	scic->completion_queue = dmam_alloc_coherent(dev,
			scic->completion_queue_entries * sizeof(u32),
			&dma_handle, GFP_KERNEL);
	if (!scic->completion_queue)
		return -ENOMEM;

	writel(lower_32_bits(dma_handle),
		&scic->smu_registers->completion_queue_lower);
	writel(upper_32_bits(dma_handle),
		&scic->smu_registers->completion_queue_upper);

	scic->remote_node_context_table = dmam_alloc_coherent(dev,
			scic->remote_node_entries *
				sizeof(union scu_remote_node_context),
			&dma_handle, GFP_KERNEL);
	if (!scic->remote_node_context_table)
		return -ENOMEM;

	writel(lower_32_bits(dma_handle),
		&scic->smu_registers->remote_node_context_lower);
	writel(upper_32_bits(dma_handle),
		&scic->smu_registers->remote_node_context_upper);

	scic->task_context_table = dmam_alloc_coherent(dev,
			scic->task_context_entries *
				sizeof(struct scu_task_context),
			&dma_handle, GFP_KERNEL);
	if (!scic->task_context_table)
		return -ENOMEM;

	writel(lower_32_bits(dma_handle),
		&scic->smu_registers->host_task_table_lower);
	writel(upper_32_bits(dma_handle),
		&scic->smu_registers->host_task_table_upper);

	result = scic_sds_unsolicited_frame_control_construct(scic);
	if (result)
		return result;

	/*
	 * Inform the silicon as to the location of the UF headers and
	 * address table.
	 */
	writel(lower_32_bits(scic->uf_control.headers.physical_address),
		&scic->scu_registers->sdma.uf_header_base_address_lower);
	writel(upper_32_bits(scic->uf_control.headers.physical_address),
		&scic->scu_registers->sdma.uf_header_base_address_upper);

	writel(lower_32_bits(scic->uf_control.address_table.physical_address),
		&scic->scu_registers->sdma.uf_address_table_lower);
	writel(upper_32_bits(scic->uf_control.address_table.physical_address),
		&scic->scu_registers->sdma.uf_address_table_upper);

	return 0;
}

/**
 * This method initializes the task context data for the controller.
 * @scic:
 *
 */
static void
scic_sds_controller_assign_task_entries(struct scic_sds_controller *controller)
{
	u32 task_assignment;

	/*
	 * Assign all the TCs to function 0
	 * TODO: Do we actually need to read this register to write it back?
	 */

	task_assignment =
		readl(&controller->smu_registers->task_context_assignment[0]);

	task_assignment |= (SMU_TCA_GEN_VAL(STARTING, 0)) |
		(SMU_TCA_GEN_VAL(ENDING,  controller->task_context_entries - 1)) |
		(SMU_TCA_GEN_BIT(RANGE_CHECK_ENABLE));

	writel(task_assignment,
		&controller->smu_registers->task_context_assignment[0]);

}

/**
 * This method initializes the hardware completion queue.
 *
 *
 */
static void scic_sds_controller_initialize_completion_queue(
	struct scic_sds_controller *scic)
{
	u32 index;
	u32 completion_queue_control_value;
	u32 completion_queue_get_value;
	u32 completion_queue_put_value;

	scic->completion_queue_get = 0;

	completion_queue_control_value = (
		SMU_CQC_QUEUE_LIMIT_SET(scic->completion_queue_entries - 1)
		| SMU_CQC_EVENT_LIMIT_SET(scic->completion_event_entries - 1)
		);

	writel(completion_queue_control_value,
	       &scic->smu_registers->completion_queue_control);


	/* Set the completion queue get pointer and enable the queue */
	completion_queue_get_value = (
		(SMU_CQGR_GEN_VAL(POINTER, 0))
		| (SMU_CQGR_GEN_VAL(EVENT_POINTER, 0))
		| (SMU_CQGR_GEN_BIT(ENABLE))
		| (SMU_CQGR_GEN_BIT(EVENT_ENABLE))
		);

	writel(completion_queue_get_value,
	       &scic->smu_registers->completion_queue_get);

	/* Set the completion queue put pointer */
	completion_queue_put_value = (
		(SMU_CQPR_GEN_VAL(POINTER, 0))
		| (SMU_CQPR_GEN_VAL(EVENT_POINTER, 0))
		);

	writel(completion_queue_put_value,
	       &scic->smu_registers->completion_queue_put);

	/* Initialize the cycle bit of the completion queue entries */
	for (index = 0; index < scic->completion_queue_entries; index++) {
		/*
		 * If get.cycle_bit != completion_queue.cycle_bit
		 * its not a valid completion queue entry
		 * so at system start all entries are invalid */
		scic->completion_queue[index] = 0x80000000;
	}
}

/**
 * This method initializes the hardware unsolicited frame queue.
 *
 *
 */
static void scic_sds_controller_initialize_unsolicited_frame_queue(
	struct scic_sds_controller *scic)
{
	u32 frame_queue_control_value;
	u32 frame_queue_get_value;
	u32 frame_queue_put_value;

	/* Write the queue size */
	frame_queue_control_value =
		SCU_UFQC_GEN_VAL(QUEUE_SIZE,
				 scic->uf_control.address_table.count);

	writel(frame_queue_control_value,
	       &scic->scu_registers->sdma.unsolicited_frame_queue_control);

	/* Setup the get pointer for the unsolicited frame queue */
	frame_queue_get_value = (
		SCU_UFQGP_GEN_VAL(POINTER, 0)
		|  SCU_UFQGP_GEN_BIT(ENABLE_BIT)
		);

	writel(frame_queue_get_value,
	       &scic->scu_registers->sdma.unsolicited_frame_get_pointer);
	/* Setup the put pointer for the unsolicited frame queue */
	frame_queue_put_value = SCU_UFQPP_GEN_VAL(POINTER, 0);
	writel(frame_queue_put_value,
	       &scic->scu_registers->sdma.unsolicited_frame_put_pointer);
}

/**
 * This method enables the hardware port task scheduler.
 *
 *
 */
static void scic_sds_controller_enable_port_task_scheduler(
	struct scic_sds_controller *scic)
{
	u32 port_task_scheduler_value;

	port_task_scheduler_value =
		readl(&scic->scu_registers->peg0.ptsg.control);
	port_task_scheduler_value |=
		(SCU_PTSGCR_GEN_BIT(ETM_ENABLE) |
		 SCU_PTSGCR_GEN_BIT(PTSG_ENABLE));
	writel(port_task_scheduler_value,
	       &scic->scu_registers->peg0.ptsg.control);
}

/**
 *
 *
 * This macro is used to delay between writes to the AFE registers during AFE
 * initialization.
 */
#define AFE_REGISTER_WRITE_DELAY 10

/* Initialize the AFE for this phy index. We need to read the AFE setup from
 * the OEM parameters none
 */
static void scic_sds_controller_afe_initialization(struct scic_sds_controller *scic)
{
	const struct scic_sds_oem_params *oem = &scic->oem_parameters.sds1;
	u32 afe_status;
	u32 phy_id;

	/* Clear DFX Status registers */
	writel(0x0081000f, &scic->scu_registers->afe.afe_dfx_master_control0);
	udelay(AFE_REGISTER_WRITE_DELAY);

	if (is_b0()) {
		/* PM Rx Equalization Save, PM SPhy Rx Acknowledgement
		 * Timer, PM Stagger Timer */
		writel(0x0007BFFF, &scic->scu_registers->afe.afe_pmsn_master_control2);
		udelay(AFE_REGISTER_WRITE_DELAY);
	}

	/* Configure bias currents to normal */
	if (is_a0())
		writel(0x00005500, &scic->scu_registers->afe.afe_bias_control);
	else if (is_a2())
		writel(0x00005A00, &scic->scu_registers->afe.afe_bias_control);
	else if (is_b0())
		writel(0x00005F00, &scic->scu_registers->afe.afe_bias_control);

	udelay(AFE_REGISTER_WRITE_DELAY);

	/* Enable PLL */
	if (is_b0())
		writel(0x80040A08, &scic->scu_registers->afe.afe_pll_control0);
	else
		writel(0x80040908, &scic->scu_registers->afe.afe_pll_control0);

	udelay(AFE_REGISTER_WRITE_DELAY);

	/* Wait for the PLL to lock */
	do {
		afe_status = readl(&scic->scu_registers->afe.afe_common_block_status);
		udelay(AFE_REGISTER_WRITE_DELAY);
	} while ((afe_status & 0x00001000) == 0);

	if (is_a0() || is_a2()) {
		/* Shorten SAS SNW lock time (RxLock timer value from 76 us to 50 us) */
		writel(0x7bcc96ad, &scic->scu_registers->afe.afe_pmsn_master_control0);
		udelay(AFE_REGISTER_WRITE_DELAY);
	}

	for (phy_id = 0; phy_id < SCI_MAX_PHYS; phy_id++) {
		const struct sci_phy_oem_params *oem_phy = &oem->phys[phy_id];

		if (is_b0()) {
			 /* Configure transmitter SSC parameters */
			writel(0x00030000, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_tx_ssc_control);
			udelay(AFE_REGISTER_WRITE_DELAY);
		} else {
			/*
			 * All defaults, except the Receive Word Alignament/Comma Detect
			 * Enable....(0xe800) */
			writel(0x00004512, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_xcvr_control0);
			udelay(AFE_REGISTER_WRITE_DELAY);

			writel(0x0050100F, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_xcvr_control1);
			udelay(AFE_REGISTER_WRITE_DELAY);
		}

		/*
		 * Power up TX and RX out from power down (PWRDNTX and PWRDNRX)
		 * & increase TX int & ext bias 20%....(0xe85c) */
		if (is_a0())
			writel(0x000003D4, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_channel_control);
		else if (is_a2())
			writel(0x000003F0, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_channel_control);
		else {
			 /* Power down TX and RX (PWRDNTX and PWRDNRX) */
			writel(0x000003d7, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_channel_control);
			udelay(AFE_REGISTER_WRITE_DELAY);

			/*
			 * Power up TX and RX out from power down (PWRDNTX and PWRDNRX)
			 * & increase TX int & ext bias 20%....(0xe85c) */
			writel(0x000003d4, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_channel_control);
		}
		udelay(AFE_REGISTER_WRITE_DELAY);

		if (is_a0() || is_a2()) {
			/* Enable TX equalization (0xe824) */
			writel(0x00040000, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_tx_control);
			udelay(AFE_REGISTER_WRITE_DELAY);
		}

		/*
		 * RDPI=0x0(RX Power On), RXOOBDETPDNC=0x0, TPD=0x0(TX Power On),
		 * RDD=0x0(RX Detect Enabled) ....(0xe800) */
		writel(0x00004100, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_xcvr_control0);
		udelay(AFE_REGISTER_WRITE_DELAY);

		/* Leave DFE/FFE on */
		if (is_a0())
			writel(0x3F09983F, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_rx_ssc_control0);
		else if (is_a2())
			writel(0x3F11103F, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_rx_ssc_control0);
		else {
			writel(0x3F11103F, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_rx_ssc_control0);
			udelay(AFE_REGISTER_WRITE_DELAY);
			/* Enable TX equalization (0xe824) */
			writel(0x00040000, &scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_tx_control);
		}
		udelay(AFE_REGISTER_WRITE_DELAY);

		writel(oem_phy->afe_tx_amp_control0,
			&scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_tx_amp_control0);
		udelay(AFE_REGISTER_WRITE_DELAY);

		writel(oem_phy->afe_tx_amp_control1,
			&scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_tx_amp_control1);
		udelay(AFE_REGISTER_WRITE_DELAY);

		writel(oem_phy->afe_tx_amp_control2,
			&scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_tx_amp_control2);
		udelay(AFE_REGISTER_WRITE_DELAY);

		writel(oem_phy->afe_tx_amp_control3,
			&scic->scu_registers->afe.scu_afe_xcvr[phy_id].afe_tx_amp_control3);
		udelay(AFE_REGISTER_WRITE_DELAY);
	}

	/* Transfer control to the PEs */
	writel(0x00010f00, &scic->scu_registers->afe.afe_dfx_master_control0);
	udelay(AFE_REGISTER_WRITE_DELAY);
}

/*
 * ****************************************************************************-
 * * SCIC SDS Controller Internal Start/Stop Routines
 * ****************************************************************************- */


/**
 * This method will attempt to transition into the ready state for the
 *    controller and indicate that the controller start operation has completed
 *    if all criteria are met.
 * @scic: This parameter indicates the controller object for which
 *    to transition to ready.
 * @status: This parameter indicates the status value to be pass into the call
 *    to scic_cb_controller_start_complete().
 *
 * none.
 */
static void scic_sds_controller_transition_to_ready(
	struct scic_sds_controller *scic,
	enum sci_status status)
{
	struct isci_host *ihost = sci_object_get_association(scic);

	if (scic->state_machine.current_state_id ==
	    SCI_BASE_CONTROLLER_STATE_STARTING) {
		/*
		 * We move into the ready state, because some of the phys/ports
		 * may be up and operational.
		 */
		sci_base_state_machine_change_state(&scic->state_machine,
						    SCI_BASE_CONTROLLER_STATE_READY);

		isci_host_start_complete(ihost, status);
	}
}

static void scic_sds_controller_timeout_handler(void *_scic)
{
	struct scic_sds_controller *scic = _scic;
	struct isci_host *ihost = sci_object_get_association(scic);
	struct sci_base_state_machine *sm = &scic->state_machine;

	if (sm->current_state_id == SCI_BASE_CONTROLLER_STATE_STARTING)
		scic_sds_controller_transition_to_ready(scic, SCI_FAILURE_TIMEOUT);
	else if (sm->current_state_id == SCI_BASE_CONTROLLER_STATE_STOPPING) {
		sci_base_state_machine_change_state(sm, SCI_BASE_CONTROLLER_STATE_FAILED);
		isci_host_stop_complete(ihost, SCI_FAILURE_TIMEOUT);
	} else	/* / @todo Now what do we want to do in this case? */
		dev_err(scic_to_dev(scic),
			"%s: Controller timer fired when controller was not "
			"in a state being timed.\n",
			__func__);
}

static enum sci_status scic_sds_controller_stop_ports(struct scic_sds_controller *scic)
{
	u32 index;
	enum sci_status port_status;
	enum sci_status status = SCI_SUCCESS;

	for (index = 0; index < scic->logical_port_entries; index++) {
		struct scic_sds_port *sci_port = &scic->port_table[index];
		scic_sds_port_handler_t stop;

		stop = sci_port->state_handlers->stop_handler;
		port_status = stop(sci_port);

		if ((port_status != SCI_SUCCESS) &&
		    (port_status != SCI_FAILURE_INVALID_STATE)) {
			status = SCI_FAILURE;

			dev_warn(scic_to_dev(scic),
				 "%s: Controller stop operation failed to "
				 "stop port %d because of status %d.\n",
				 __func__,
				 sci_port->logical_port_index,
				 port_status);
		}
	}

	return status;
}

static inline void scic_sds_controller_phy_timer_start(
		struct scic_sds_controller *scic)
{
	isci_timer_start(scic->phy_startup_timer,
			 SCIC_SDS_CONTROLLER_PHY_START_TIMEOUT);

	scic->phy_startup_timer_pending = true;
}

static void scic_sds_controller_phy_timer_stop(struct scic_sds_controller *scic)
{
	isci_timer_stop(scic->phy_startup_timer);

	scic->phy_startup_timer_pending = false;
}

/**
 * scic_sds_controller_start_next_phy - start phy
 * @scic: controller
 *
 * If all the phys have been started, then attempt to transition the
 * controller to the READY state and inform the user
 * (scic_cb_controller_start_complete()).
 */
static enum sci_status scic_sds_controller_start_next_phy(struct scic_sds_controller *scic)
{
	struct scic_sds_oem_params *oem = &scic->oem_parameters.sds1;
	struct scic_sds_phy *sci_phy;
	enum sci_status status;

	status = SCI_SUCCESS;

	if (scic->phy_startup_timer_pending)
		return status;

	if (scic->next_phy_to_start >= SCI_MAX_PHYS) {
		bool is_controller_start_complete = true;
		u32 state;
		u8 index;

		for (index = 0; index < SCI_MAX_PHYS; index++) {
			sci_phy = &scic->phy_table[index];
			state = sci_phy->state_machine.current_state_id;

			if (!scic_sds_phy_get_port(sci_phy))
				continue;

			/* The controller start operation is complete iff:
			 * - all links have been given an opportunity to start
			 * - have no indication of a connected device
			 * - have an indication of a connected device and it has
			 *   finished the link training process.
			 */
			if ((sci_phy->is_in_link_training == false &&
			     state == SCI_BASE_PHY_STATE_INITIAL) ||
			    (sci_phy->is_in_link_training == false &&
			     state == SCI_BASE_PHY_STATE_STOPPED) ||
			    (sci_phy->is_in_link_training == true &&
			     state == SCI_BASE_PHY_STATE_STARTING)) {
				is_controller_start_complete = false;
				break;
			}
		}

		/*
		 * The controller has successfully finished the start process.
		 * Inform the SCI Core user and transition to the READY state. */
		if (is_controller_start_complete == true) {
			scic_sds_controller_transition_to_ready(scic, SCI_SUCCESS);
			scic_sds_controller_phy_timer_stop(scic);
		}
	} else {
		sci_phy = &scic->phy_table[scic->next_phy_to_start];

		if (oem->controller.mode_type == SCIC_PORT_MANUAL_CONFIGURATION_MODE) {
			if (scic_sds_phy_get_port(sci_phy) == NULL) {
				scic->next_phy_to_start++;

				/* Caution recursion ahead be forwarned
				 *
				 * The PHY was never added to a PORT in MPC mode
				 * so start the next phy in sequence This phy
				 * will never go link up and will not draw power
				 * the OEM parameters either configured the phy
				 * incorrectly for the PORT or it was never
				 * assigned to a PORT
				 */
				return scic_sds_controller_start_next_phy(scic);
			}
		}

		status = scic_sds_phy_start(sci_phy);

		if (status == SCI_SUCCESS) {
			scic_sds_controller_phy_timer_start(scic);
		} else {
			dev_warn(scic_to_dev(scic),
				 "%s: Controller stop operation failed "
				 "to stop phy %d because of status "
				 "%d.\n",
				 __func__,
				 scic->phy_table[scic->next_phy_to_start].phy_index,
				 status);
		}

		scic->next_phy_to_start++;
	}

	return status;
}

static void scic_sds_controller_phy_startup_timeout_handler(void *_scic)
{
	struct scic_sds_controller *scic = _scic;
	enum sci_status status;

	scic->phy_startup_timer_pending = false;
	status = SCI_FAILURE;
	while (status != SCI_SUCCESS)
		status = scic_sds_controller_start_next_phy(scic);
}

static enum sci_status scic_sds_controller_initialize_phy_startup(struct scic_sds_controller *scic)
{
	struct isci_host *ihost = sci_object_get_association(scic);

	scic->phy_startup_timer = isci_timer_create(ihost,
						    scic,
						    scic_sds_controller_phy_startup_timeout_handler);

	if (scic->phy_startup_timer == NULL)
		return SCI_FAILURE_INSUFFICIENT_RESOURCES;
	else {
		scic->next_phy_to_start = 0;
		scic->phy_startup_timer_pending = false;
	}

	return SCI_SUCCESS;
}

static enum sci_status scic_sds_controller_stop_phys(struct scic_sds_controller *scic)
{
	u32 index;
	enum sci_status status;
	enum sci_status phy_status;

	status = SCI_SUCCESS;

	for (index = 0; index < SCI_MAX_PHYS; index++) {
		phy_status = scic_sds_phy_stop(&scic->phy_table[index]);

		if (
			(phy_status != SCI_SUCCESS)
			&& (phy_status != SCI_FAILURE_INVALID_STATE)
			) {
			status = SCI_FAILURE;

			dev_warn(scic_to_dev(scic),
				 "%s: Controller stop operation failed to stop "
				 "phy %d because of status %d.\n",
				 __func__,
				 scic->phy_table[index].phy_index, phy_status);
		}
	}

	return status;
}

static enum sci_status scic_sds_controller_stop_devices(struct scic_sds_controller *scic)
{
	u32 index;
	enum sci_status status;
	enum sci_status device_status;

	status = SCI_SUCCESS;

	for (index = 0; index < scic->remote_node_entries; index++) {
		if (scic->device_table[index] != NULL) {
			/* / @todo What timeout value do we want to provide to this request? */
			device_status = scic_remote_device_stop(scic->device_table[index], 0);

			if ((device_status != SCI_SUCCESS) &&
			    (device_status != SCI_FAILURE_INVALID_STATE)) {
				dev_warn(scic_to_dev(scic),
					 "%s: Controller stop operation failed "
					 "to stop device 0x%p because of "
					 "status %d.\n",
					 __func__,
					 scic->device_table[index], device_status);
			}
		}
	}

	return status;
}

static void scic_sds_controller_power_control_timer_start(struct scic_sds_controller *scic)
{
	isci_timer_start(scic->power_control.timer,
			 SCIC_SDS_CONTROLLER_POWER_CONTROL_INTERVAL);

	scic->power_control.timer_started = true;
}

static void scic_sds_controller_power_control_timer_stop(struct scic_sds_controller *scic)
{
	if (scic->power_control.timer_started) {
		isci_timer_stop(scic->power_control.timer);
		scic->power_control.timer_started = false;
	}
}

static void scic_sds_controller_power_control_timer_restart(struct scic_sds_controller *scic)
{
	scic_sds_controller_power_control_timer_stop(scic);
	scic_sds_controller_power_control_timer_start(scic);
}

static void scic_sds_controller_power_control_timer_handler(
	void *controller)
{
	struct scic_sds_controller *scic;

	scic = (struct scic_sds_controller *)controller;

	scic->power_control.phys_granted_power = 0;

	if (scic->power_control.phys_waiting == 0) {
		scic->power_control.timer_started = false;
	} else {
		struct scic_sds_phy *sci_phy = NULL;
		u8 i;

		for (i = 0;
		     (i < SCI_MAX_PHYS)
		     && (scic->power_control.phys_waiting != 0);
		     i++) {
			if (scic->power_control.requesters[i] != NULL) {
				if (scic->power_control.phys_granted_power <
				    scic->oem_parameters.sds1.controller.max_concurrent_dev_spin_up) {
					sci_phy = scic->power_control.requesters[i];
					scic->power_control.requesters[i] = NULL;
					scic->power_control.phys_waiting--;
					scic->power_control.phys_granted_power++;
					scic_sds_phy_consume_power_handler(sci_phy);
				} else {
					break;
				}
			}
		}

		/*
		 * It doesn't matter if the power list is empty, we need to start the
		 * timer in case another phy becomes ready.
		 */
		scic_sds_controller_power_control_timer_start(scic);
	}
}

/**
 * This method inserts the phy in the stagger spinup control queue.
 * @scic:
 *
 *
 */
void scic_sds_controller_power_control_queue_insert(
	struct scic_sds_controller *scic,
	struct scic_sds_phy *sci_phy)
{
	BUG_ON(sci_phy == NULL);

	if (scic->power_control.phys_granted_power <
	    scic->oem_parameters.sds1.controller.max_concurrent_dev_spin_up) {
		scic->power_control.phys_granted_power++;
		scic_sds_phy_consume_power_handler(sci_phy);

		/*
		 * stop and start the power_control timer. When the timer fires, the
		 * no_of_phys_granted_power will be set to 0
		 */
		scic_sds_controller_power_control_timer_restart(scic);
	} else {
		/* Add the phy in the waiting list */
		scic->power_control.requesters[sci_phy->phy_index] = sci_phy;
		scic->power_control.phys_waiting++;
	}
}

/**
 * This method removes the phy from the stagger spinup control queue.
 * @scic:
 *
 *
 */
void scic_sds_controller_power_control_queue_remove(
	struct scic_sds_controller *scic,
	struct scic_sds_phy *sci_phy)
{
	BUG_ON(sci_phy == NULL);

	if (scic->power_control.requesters[sci_phy->phy_index] != NULL) {
		scic->power_control.phys_waiting--;
	}

	scic->power_control.requesters[sci_phy->phy_index] = NULL;
}

/*
 * ****************************************************************************-
 * * SCIC SDS Controller Completion Routines
 * ****************************************************************************- */

/**
 * This method returns a true value if the completion queue has entries that
 *    can be processed
 * @scic:
 *
 * bool true if the completion queue has entries to process false if the
 * completion queue has no entries to process
 */
static bool scic_sds_controller_completion_queue_has_entries(
	struct scic_sds_controller *scic)
{
	u32 get_value = scic->completion_queue_get;
	u32 get_index = get_value & SMU_COMPLETION_QUEUE_GET_POINTER_MASK;

	if (NORMALIZE_GET_POINTER_CYCLE_BIT(get_value) ==
	    COMPLETION_QUEUE_CYCLE_BIT(scic->completion_queue[get_index]))
		return true;

	return false;
}

/**
 * This method processes a task completion notification.  This is called from
 *    within the controller completion handler.
 * @scic:
 * @completion_entry:
 *
 */
static void scic_sds_controller_task_completion(
	struct scic_sds_controller *scic,
	u32 completion_entry)
{
	u32 index;
	struct scic_sds_request *io_request;

	index = SCU_GET_COMPLETION_INDEX(completion_entry);
	io_request = scic->io_request_table[index];

	/* Make sure that we really want to process this IO request */
	if (
		(io_request != NULL)
		&& (io_request->io_tag != SCI_CONTROLLER_INVALID_IO_TAG)
		&& (
			scic_sds_io_tag_get_sequence(io_request->io_tag)
			== scic->io_request_sequence[index]
			)
		) {
		/* Yep this is a valid io request pass it along to the io request handler */
		scic_sds_io_request_tc_completion(io_request, completion_entry);
	}
}

/**
 * This method processes an SDMA completion event.  This is called from within
 *    the controller completion handler.
 * @scic:
 * @completion_entry:
 *
 */
static void scic_sds_controller_sdma_completion(
	struct scic_sds_controller *scic,
	u32 completion_entry)
{
	u32 index;
	struct scic_sds_request *io_request;
	struct scic_sds_remote_device *device;

	index = SCU_GET_COMPLETION_INDEX(completion_entry);

	switch (scu_get_command_request_type(completion_entry)) {
	case SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_TC:
	case SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_TC:
		io_request = scic->io_request_table[index];
		dev_warn(scic_to_dev(scic),
			 "%s: SCIC SDS Completion type SDMA %x for io request "
			 "%p\n",
			 __func__,
			 completion_entry,
			 io_request);
		/* @todo For a post TC operation we need to fail the IO
		 * request
		 */
		break;

	case SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_RNC:
	case SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC:
	case SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_RNC:
		device = scic->device_table[index];
		dev_warn(scic_to_dev(scic),
			 "%s: SCIC SDS Completion type SDMA %x for remote "
			 "device %p\n",
			 __func__,
			 completion_entry,
			 device);
		/* @todo For a port RNC operation we need to fail the
		 * device
		 */
		break;

	default:
		dev_warn(scic_to_dev(scic),
			 "%s: SCIC SDS Completion unknown SDMA completion "
			 "type %x\n",
			 __func__,
			 completion_entry);
		break;

	}
}

/**
 *
 * @scic:
 * @completion_entry:
 *
 * This method processes an unsolicited frame message.  This is called from
 * within the controller completion handler. none
 */
static void scic_sds_controller_unsolicited_frame(
	struct scic_sds_controller *scic,
	u32 completion_entry)
{
	u32 index;
	u32 frame_index;

	struct scu_unsolicited_frame_header *frame_header;
	struct scic_sds_phy *phy;
	struct scic_sds_remote_device *device;

	enum sci_status result = SCI_FAILURE;

	frame_index = SCU_GET_FRAME_INDEX(completion_entry);

	frame_header
		= scic->uf_control.buffers.array[frame_index].header;
	scic->uf_control.buffers.array[frame_index].state
		= UNSOLICITED_FRAME_IN_USE;

	if (SCU_GET_FRAME_ERROR(completion_entry)) {
		/*
		 * / @todo If the IAF frame or SIGNATURE FIS frame has an error will
		 * /       this cause a problem? We expect the phy initialization will
		 * /       fail if there is an error in the frame. */
		scic_sds_controller_release_frame(scic, frame_index);
		return;
	}

	if (frame_header->is_address_frame) {
		index = SCU_GET_PROTOCOL_ENGINE_INDEX(completion_entry);
		phy = &scic->phy_table[index];
		if (phy != NULL) {
			result = scic_sds_phy_frame_handler(phy, frame_index);
		}
	} else {

		index = SCU_GET_COMPLETION_INDEX(completion_entry);

		if (index == SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX) {
			/*
			 * This is a signature fis or a frame from a direct attached SATA
			 * device that has not yet been created.  In either case forwared
			 * the frame to the PE and let it take care of the frame data. */
			index = SCU_GET_PROTOCOL_ENGINE_INDEX(completion_entry);
			phy = &scic->phy_table[index];
			result = scic_sds_phy_frame_handler(phy, frame_index);
		} else {
			if (index < scic->remote_node_entries)
				device = scic->device_table[index];
			else
				device = NULL;

			if (device != NULL)
				result = scic_sds_remote_device_frame_handler(device, frame_index);
			else
				scic_sds_controller_release_frame(scic, frame_index);
		}
	}

	if (result != SCI_SUCCESS) {
		/*
		 * / @todo Is there any reason to report some additional error message
		 * /       when we get this failure notifiction? */
	}
}

/**
 * This method processes an event completion entry.  This is called from within
 *    the controller completion handler.
 * @scic:
 * @completion_entry:
 *
 */
static void scic_sds_controller_event_completion(
	struct scic_sds_controller *scic,
	u32 completion_entry)
{
	u32 index;
	struct scic_sds_request *io_request;
	struct scic_sds_remote_device *device;
	struct scic_sds_phy *phy;

	index = SCU_GET_COMPLETION_INDEX(completion_entry);

	switch (scu_get_event_type(completion_entry)) {
	case SCU_EVENT_TYPE_SMU_COMMAND_ERROR:
		/* / @todo The driver did something wrong and we need to fix the condtion. */
		dev_err(scic_to_dev(scic),
			"%s: SCIC Controller 0x%p received SMU command error "
			"0x%x\n",
			__func__,
			scic,
			completion_entry);
		break;

	case SCU_EVENT_TYPE_SMU_PCQ_ERROR:
	case SCU_EVENT_TYPE_SMU_ERROR:
	case SCU_EVENT_TYPE_FATAL_MEMORY_ERROR:
		/*
		 * / @todo This is a hardware failure and its likely that we want to
		 * /       reset the controller. */
		dev_err(scic_to_dev(scic),
			"%s: SCIC Controller 0x%p received fatal controller "
			"event  0x%x\n",
			__func__,
			scic,
			completion_entry);
		break;

	case SCU_EVENT_TYPE_TRANSPORT_ERROR:
		io_request = scic->io_request_table[index];
		scic_sds_io_request_event_handler(io_request, completion_entry);
		break;

	case SCU_EVENT_TYPE_PTX_SCHEDULE_EVENT:
		switch (scu_get_event_specifier(completion_entry)) {
		case SCU_EVENT_SPECIFIC_SMP_RESPONSE_NO_PE:
		case SCU_EVENT_SPECIFIC_TASK_TIMEOUT:
			io_request = scic->io_request_table[index];
			if (io_request != NULL)
				scic_sds_io_request_event_handler(io_request, completion_entry);
			else
				dev_warn(scic_to_dev(scic),
					 "%s: SCIC Controller 0x%p received "
					 "event 0x%x for io request object "
					 "that doesnt exist.\n",
					 __func__,
					 scic,
					 completion_entry);

			break;

		case SCU_EVENT_SPECIFIC_IT_NEXUS_TIMEOUT:
			device = scic->device_table[index];
			if (device != NULL)
				scic_sds_remote_device_event_handler(device, completion_entry);
			else
				dev_warn(scic_to_dev(scic),
					 "%s: SCIC Controller 0x%p received "
					 "event 0x%x for remote device object "
					 "that doesnt exist.\n",
					 __func__,
					 scic,
					 completion_entry);

			break;
		}
		break;

	case SCU_EVENT_TYPE_BROADCAST_CHANGE:
	/*
	 * direct the broadcast change event to the phy first and then let
	 * the phy redirect the broadcast change to the port object */
	case SCU_EVENT_TYPE_ERR_CNT_EVENT:
	/*
	 * direct error counter event to the phy object since that is where
	 * we get the event notification.  This is a type 4 event. */
	case SCU_EVENT_TYPE_OSSP_EVENT:
		index = SCU_GET_PROTOCOL_ENGINE_INDEX(completion_entry);
		phy = &scic->phy_table[index];
		scic_sds_phy_event_handler(phy, completion_entry);
		break;

	case SCU_EVENT_TYPE_RNC_SUSPEND_TX:
	case SCU_EVENT_TYPE_RNC_SUSPEND_TX_RX:
	case SCU_EVENT_TYPE_RNC_OPS_MISC:
		if (index < scic->remote_node_entries) {
			device = scic->device_table[index];

			if (device != NULL)
				scic_sds_remote_device_event_handler(device, completion_entry);
		} else
			dev_err(scic_to_dev(scic),
				"%s: SCIC Controller 0x%p received event 0x%x "
				"for remote device object 0x%0x that doesnt "
				"exist.\n",
				__func__,
				scic,
				completion_entry,
				index);

		break;

	default:
		dev_warn(scic_to_dev(scic),
			 "%s: SCIC Controller received unknown event code %x\n",
			 __func__,
			 completion_entry);
		break;
	}
}

/**
 * This method is a private routine for processing the completion queue entries.
 * @scic:
 *
 */
static void scic_sds_controller_process_completions(
	struct scic_sds_controller *scic)
{
	u32 completion_count = 0;
	u32 completion_entry;
	u32 get_index;
	u32 get_cycle;
	u32 event_index;
	u32 event_cycle;

	dev_dbg(scic_to_dev(scic),
		"%s: completion queue begining get:0x%08x\n",
		__func__,
		scic->completion_queue_get);

	/* Get the component parts of the completion queue */
	get_index = NORMALIZE_GET_POINTER(scic->completion_queue_get);
	get_cycle = SMU_CQGR_CYCLE_BIT & scic->completion_queue_get;

	event_index = NORMALIZE_EVENT_POINTER(scic->completion_queue_get);
	event_cycle = SMU_CQGR_EVENT_CYCLE_BIT & scic->completion_queue_get;

	while (
		NORMALIZE_GET_POINTER_CYCLE_BIT(get_cycle)
		== COMPLETION_QUEUE_CYCLE_BIT(scic->completion_queue[get_index])
		) {
		completion_count++;

		completion_entry = scic->completion_queue[get_index];
		INCREMENT_COMPLETION_QUEUE_GET(scic, get_index, get_cycle);

		dev_dbg(scic_to_dev(scic),
			"%s: completion queue entry:0x%08x\n",
			__func__,
			completion_entry);

		switch (SCU_GET_COMPLETION_TYPE(completion_entry)) {
		case SCU_COMPLETION_TYPE_TASK:
			scic_sds_controller_task_completion(scic, completion_entry);
			break;

		case SCU_COMPLETION_TYPE_SDMA:
			scic_sds_controller_sdma_completion(scic, completion_entry);
			break;

		case SCU_COMPLETION_TYPE_UFI:
			scic_sds_controller_unsolicited_frame(scic, completion_entry);
			break;

		case SCU_COMPLETION_TYPE_EVENT:
			INCREMENT_EVENT_QUEUE_GET(scic, event_index, event_cycle);
			scic_sds_controller_event_completion(scic, completion_entry);
			break;

		case SCU_COMPLETION_TYPE_NOTIFY:
			/*
			 * Presently we do the same thing with a notify event that we do with the
			 * other event codes. */
			INCREMENT_EVENT_QUEUE_GET(scic, event_index, event_cycle);
			scic_sds_controller_event_completion(scic, completion_entry);
			break;

		default:
			dev_warn(scic_to_dev(scic),
				 "%s: SCIC Controller received unknown "
				 "completion type %x\n",
				 __func__,
				 completion_entry);
			break;
		}
	}

	/* Update the get register if we completed one or more entries */
	if (completion_count > 0) {
		scic->completion_queue_get =
			SMU_CQGR_GEN_BIT(ENABLE) |
			SMU_CQGR_GEN_BIT(EVENT_ENABLE) |
			event_cycle |
			SMU_CQGR_GEN_VAL(EVENT_POINTER, event_index) |
			get_cycle |
			SMU_CQGR_GEN_VAL(POINTER, get_index);

		writel(scic->completion_queue_get,
		       &scic->smu_registers->completion_queue_get);

	}

	dev_dbg(scic_to_dev(scic),
		"%s: completion queue ending get:0x%08x\n",
		__func__,
		scic->completion_queue_get);

}

bool scic_sds_controller_isr(struct scic_sds_controller *scic)
{
	if (scic_sds_controller_completion_queue_has_entries(scic)) {
		return true;
	} else {
		/*
		 * we have a spurious interrupt it could be that we have already
		 * emptied the completion queue from a previous interrupt */
		writel(SMU_ISR_COMPLETION, &scic->smu_registers->interrupt_status);

		/*
		 * There is a race in the hardware that could cause us not to be notified
		 * of an interrupt completion if we do not take this step.  We will mask
		 * then unmask the interrupts so if there is another interrupt pending
		 * the clearing of the interrupt source we get the next interrupt message. */
		writel(0xFF000000, &scic->smu_registers->interrupt_mask);
		writel(0, &scic->smu_registers->interrupt_mask);
	}

	return false;
}

void scic_sds_controller_completion_handler(struct scic_sds_controller *scic)
{
	/* Empty out the completion queue */
	if (scic_sds_controller_completion_queue_has_entries(scic))
		scic_sds_controller_process_completions(scic);

	/* Clear the interrupt and enable all interrupts again */
	writel(SMU_ISR_COMPLETION, &scic->smu_registers->interrupt_status);
	/* Could we write the value of SMU_ISR_COMPLETION? */
	writel(0xFF000000, &scic->smu_registers->interrupt_mask);
	writel(0, &scic->smu_registers->interrupt_mask);
}

bool scic_sds_controller_error_isr(struct scic_sds_controller *scic)
{
	u32 interrupt_status;

	interrupt_status =
		readl(&scic->smu_registers->interrupt_status);
	interrupt_status &= (SMU_ISR_QUEUE_ERROR | SMU_ISR_QUEUE_SUSPEND);

	if (interrupt_status != 0) {
		/*
		 * There is an error interrupt pending so let it through and handle
		 * in the callback */
		return true;
	}

	/*
	 * There is a race in the hardware that could cause us not to be notified
	 * of an interrupt completion if we do not take this step.  We will mask
	 * then unmask the error interrupts so if there was another interrupt
	 * pending we will be notified.
	 * Could we write the value of (SMU_ISR_QUEUE_ERROR | SMU_ISR_QUEUE_SUSPEND)? */
	writel(0xff, &scic->smu_registers->interrupt_mask);
	writel(0, &scic->smu_registers->interrupt_mask);

	return false;
}

void scic_sds_controller_error_handler(struct scic_sds_controller *scic)
{
	u32 interrupt_status;

	interrupt_status =
		readl(&scic->smu_registers->interrupt_status);

	if ((interrupt_status & SMU_ISR_QUEUE_SUSPEND) &&
	    scic_sds_controller_completion_queue_has_entries(scic)) {

		scic_sds_controller_process_completions(scic);
		writel(SMU_ISR_QUEUE_SUSPEND, &scic->smu_registers->interrupt_status);
	} else {
		dev_err(scic_to_dev(scic), "%s: status: %#x\n", __func__,
			interrupt_status);

		sci_base_state_machine_change_state(&scic->state_machine,
						    SCI_BASE_CONTROLLER_STATE_FAILED);

		return;
	}

	/* If we dont process any completions I am not sure that we want to do this.
	 * We are in the middle of a hardware fault and should probably be reset.
	 */
	writel(0, &scic->smu_registers->interrupt_mask);
}




void scic_sds_controller_link_up(struct scic_sds_controller *scic,
		struct scic_sds_port *port, struct scic_sds_phy *phy)
{
	switch (scic->state_machine.current_state_id) {
	case SCI_BASE_CONTROLLER_STATE_STARTING:
		scic_sds_controller_phy_timer_stop(scic);
		scic->port_agent.link_up_handler(scic, &scic->port_agent,
						 port, phy);
		scic_sds_controller_start_next_phy(scic);
		break;
	case SCI_BASE_CONTROLLER_STATE_READY:
		scic->port_agent.link_up_handler(scic, &scic->port_agent,
						 port, phy);
		break;
	default:
		dev_dbg(scic_to_dev(scic),
			"%s: SCIC Controller linkup event from phy %d in "
			"unexpected state %d\n", __func__, phy->phy_index,
			scic->state_machine.current_state_id);
	}
}

void scic_sds_controller_link_down(struct scic_sds_controller *scic,
		struct scic_sds_port *port, struct scic_sds_phy *phy)
{
	switch (scic->state_machine.current_state_id) {
	case SCI_BASE_CONTROLLER_STATE_STARTING:
	case SCI_BASE_CONTROLLER_STATE_READY:
		scic->port_agent.link_down_handler(scic, &scic->port_agent,
						   port, phy);
		break;
	default:
		dev_dbg(scic_to_dev(scic),
			"%s: SCIC Controller linkdown event from phy %d in "
			"unexpected state %d\n",
			__func__,
			phy->phy_index,
			scic->state_machine.current_state_id);
	}
}

/**
 * This is a helper method to determine if any remote devices on this
 * controller are still in the stopping state.
 *
 */
static bool scic_sds_controller_has_remote_devices_stopping(
	struct scic_sds_controller *controller)
{
	u32 index;

	for (index = 0; index < controller->remote_node_entries; index++) {
		if ((controller->device_table[index] != NULL) &&
		   (controller->device_table[index]->state_machine.current_state_id
		    == SCI_BASE_REMOTE_DEVICE_STATE_STOPPING))
			return true;
	}

	return false;
}

/**
 * This method is called by the remote device to inform the controller
 * object that the remote device has stopped.
 */
void scic_sds_controller_remote_device_stopped(struct scic_sds_controller *scic,
					       struct scic_sds_remote_device *sci_dev)
{
	if (scic->state_machine.current_state_id !=
	    SCI_BASE_CONTROLLER_STATE_STOPPING) {
		dev_dbg(scic_to_dev(scic),
			"SCIC Controller 0x%p remote device stopped event "
			"from device 0x%p in unexpected state %d\n",
			scic, sci_dev,
			scic->state_machine.current_state_id);
		return;
	}

	if (!scic_sds_controller_has_remote_devices_stopping(scic)) {
		sci_base_state_machine_change_state(&scic->state_machine,
				SCI_BASE_CONTROLLER_STATE_STOPPED);
	}
}

/**
 * This method will write to the SCU PCP register the request value. The method
 *    is used to suspend/resume ports, devices, and phys.
 * @scic:
 *
 *
 */
void scic_sds_controller_post_request(
	struct scic_sds_controller *scic,
	u32 request)
{
	dev_dbg(scic_to_dev(scic),
		"%s: SCIC Controller 0x%p post request 0x%08x\n",
		__func__,
		scic,
		request);

	writel(request, &scic->smu_registers->post_context_port);
}

/**
 * This method will copy the soft copy of the task context into the physical
 *    memory accessible by the controller.
 * @scic: This parameter specifies the controller for which to copy
 *    the task context.
 * @sci_req: This parameter specifies the request for which the task
 *    context is being copied.
 *
 * After this call is made the SCIC_SDS_IO_REQUEST object will always point to
 * the physical memory version of the task context. Thus, all subsequent
 * updates to the task context are performed in the TC table (i.e. DMAable
 * memory). none
 */
void scic_sds_controller_copy_task_context(
	struct scic_sds_controller *scic,
	struct scic_sds_request *sci_req)
{
	struct scu_task_context *task_context_buffer;

	task_context_buffer = scic_sds_controller_get_task_context_buffer(
		scic, sci_req->io_tag);

	memcpy(task_context_buffer,
	       sci_req->task_context_buffer,
	       SCI_FIELD_OFFSET(struct scu_task_context, sgl_snapshot_ac));

	/*
	 * Now that the soft copy of the TC has been copied into the TC
	 * table accessible by the silicon.  Thus, any further changes to
	 * the TC (e.g. TC termination) occur in the appropriate location. */
	sci_req->task_context_buffer = task_context_buffer;
}

/**
 * This method returns the task context buffer for the given io tag.
 * @scic:
 * @io_tag:
 *
 * struct scu_task_context*
 */
struct scu_task_context *scic_sds_controller_get_task_context_buffer(
	struct scic_sds_controller *scic,
	u16 io_tag
	) {
	u16 task_index = scic_sds_io_tag_get_index(io_tag);

	if (task_index < scic->task_context_entries) {
		return &scic->task_context_table[task_index];
	}

	return NULL;
}

/**
 * This method returnst the sequence value from the io tag value
 * @scic:
 * @io_tag:
 *
 * u16
 */

/**
 * This method returns the IO request associated with the tag value
 * @scic:
 * @io_tag:
 *
 * SCIC_SDS_IO_REQUEST_T* NULL if there is no valid IO request at the tag value
 */
struct scic_sds_request *scic_sds_controller_get_io_request_from_tag(
	struct scic_sds_controller *scic,
	u16 io_tag
	) {
	u16 task_index;
	u16 task_sequence;

	task_index = scic_sds_io_tag_get_index(io_tag);

	if (task_index  < scic->task_context_entries) {
		if (scic->io_request_table[task_index] != NULL) {
			task_sequence = scic_sds_io_tag_get_sequence(io_tag);

			if (task_sequence == scic->io_request_sequence[task_index]) {
				return scic->io_request_table[task_index];
			}
		}
	}

	return NULL;
}

/**
 * This method allocates remote node index and the reserves the remote node
 *    context space for use. This method can fail if there are no more remote
 *    node index available.
 * @scic: This is the controller object which contains the set of
 *    free remote node ids
 * @sci_dev: This is the device object which is requesting the a remote node
 *    id
 * @node_id: This is the remote node id that is assinged to the device if one
 *    is available
 *
 * enum sci_status SCI_FAILURE_OUT_OF_RESOURCES if there are no available remote
 * node index available.
 */
enum sci_status scic_sds_controller_allocate_remote_node_context(
	struct scic_sds_controller *scic,
	struct scic_sds_remote_device *sci_dev,
	u16 *node_id)
{
	u16 node_index;
	u32 remote_node_count = scic_sds_remote_device_node_count(sci_dev);

	node_index = scic_sds_remote_node_table_allocate_remote_node(
		&scic->available_remote_nodes, remote_node_count
		);

	if (node_index != SCIC_SDS_REMOTE_NODE_CONTEXT_INVALID_INDEX) {
		scic->device_table[node_index] = sci_dev;

		*node_id = node_index;

		return SCI_SUCCESS;
	}

	return SCI_FAILURE_INSUFFICIENT_RESOURCES;
}

/**
 * This method frees the remote node index back to the available pool.  Once
 *    this is done the remote node context buffer is no longer valid and can
 *    not be used.
 * @scic:
 * @sci_dev:
 * @node_id:
 *
 */
void scic_sds_controller_free_remote_node_context(
	struct scic_sds_controller *scic,
	struct scic_sds_remote_device *sci_dev,
	u16 node_id)
{
	u32 remote_node_count = scic_sds_remote_device_node_count(sci_dev);

	if (scic->device_table[node_id] == sci_dev) {
		scic->device_table[node_id] = NULL;

		scic_sds_remote_node_table_release_remote_node_index(
			&scic->available_remote_nodes, remote_node_count, node_id
			);
	}
}

/**
 * This method returns the union scu_remote_node_context for the specified remote
 *    node id.
 * @scic:
 * @node_id:
 *
 * union scu_remote_node_context*
 */
union scu_remote_node_context *scic_sds_controller_get_remote_node_context_buffer(
	struct scic_sds_controller *scic,
	u16 node_id
	) {
	if (
		(node_id < scic->remote_node_entries)
		&& (scic->device_table[node_id] != NULL)
		) {
		return &scic->remote_node_context_table[node_id];
	}

	return NULL;
}

/**
 *
 * @resposne_buffer: This is the buffer into which the D2H register FIS will be
 *    constructed.
 * @frame_header: This is the frame header returned by the hardware.
 * @frame_buffer: This is the frame buffer returned by the hardware.
 *
 * This method will combind the frame header and frame buffer to create a SATA
 * D2H register FIS none
 */
void scic_sds_controller_copy_sata_response(
	void *response_buffer,
	void *frame_header,
	void *frame_buffer)
{
	memcpy(
		response_buffer,
		frame_header,
		sizeof(u32)
		);

	memcpy(
		(char *)((char *)response_buffer + sizeof(u32)),
		frame_buffer,
		sizeof(struct sata_fis_reg_d2h) - sizeof(u32)
		);
}

/**
 * This method releases the frame once this is done the frame is available for
 *    re-use by the hardware.  The data contained in the frame header and frame
 *    buffer is no longer valid. The UF queue get pointer is only updated if UF
 *    control indicates this is appropriate.
 * @scic:
 * @frame_index:
 *
 */
void scic_sds_controller_release_frame(
	struct scic_sds_controller *scic,
	u32 frame_index)
{
	if (scic_sds_unsolicited_frame_control_release_frame(
		    &scic->uf_control, frame_index) == true)
		writel(scic->uf_control.get,
			&scic->scu_registers->sdma.unsolicited_frame_get_pointer);
}

/**
 * This method sets user parameters and OEM parameters to default values.
 *    Users can override these values utilizing the scic_user_parameters_set()
 *    and scic_oem_parameters_set() methods.
 * @scic: This parameter specifies the controller for which to set the
 *    configuration parameters to their default values.
 *
 */
static void scic_sds_controller_set_default_config_parameters(struct scic_sds_controller *scic)
{
	struct isci_host *ihost = sci_object_get_association(scic);
	u16 index;

	/* Default to APC mode. */
	scic->oem_parameters.sds1.controller.mode_type = SCIC_PORT_AUTOMATIC_CONFIGURATION_MODE;

	/* Default to APC mode. */
	scic->oem_parameters.sds1.controller.max_concurrent_dev_spin_up = 1;

	/* Default to no SSC operation. */
	scic->oem_parameters.sds1.controller.do_enable_ssc = false;

	/* Initialize all of the port parameter information to narrow ports. */
	for (index = 0; index < SCI_MAX_PORTS; index++) {
		scic->oem_parameters.sds1.ports[index].phy_mask = 0;
	}

	/* Initialize all of the phy parameter information. */
	for (index = 0; index < SCI_MAX_PHYS; index++) {
		/* Default to 6G (i.e. Gen 3) for now. */
		scic->user_parameters.sds1.phys[index].max_speed_generation = 3;

		/* the frequencies cannot be 0 */
		scic->user_parameters.sds1.phys[index].align_insertion_frequency = 0x7f;
		scic->user_parameters.sds1.phys[index].in_connection_align_insertion_frequency = 0xff;
		scic->user_parameters.sds1.phys[index].notify_enable_spin_up_insertion_frequency = 0x33;

		/*
		 * Previous Vitesse based expanders had a arbitration issue that
		 * is worked around by having the upper 32-bits of SAS address
		 * with a value greater then the Vitesse company identifier.
		 * Hence, usage of 0x5FCFFFFF. */
		scic->oem_parameters.sds1.phys[index].sas_address.low = 0x1 + ihost->id;
		scic->oem_parameters.sds1.phys[index].sas_address.high = 0x5FCFFFFF;
	}

	scic->user_parameters.sds1.stp_inactivity_timeout = 5;
	scic->user_parameters.sds1.ssp_inactivity_timeout = 5;
	scic->user_parameters.sds1.stp_max_occupancy_timeout = 5;
	scic->user_parameters.sds1.ssp_max_occupancy_timeout = 20;
	scic->user_parameters.sds1.no_outbound_task_timeout = 20;
}

/**
 * scic_controller_get_suggested_start_timeout() - This method returns the
 *    suggested scic_controller_start() timeout amount.  The user is free to
 *    use any timeout value, but this method provides the suggested minimum
 *    start timeout value.  The returned value is based upon empirical
 *    information determined as a result of interoperability testing.
 * @controller: the handle to the controller object for which to return the
 *    suggested start timeout.
 *
 * This method returns the number of milliseconds for the suggested start
 * operation timeout.
 */
u32 scic_controller_get_suggested_start_timeout(
	struct scic_sds_controller *sc)
{
	/* Validate the user supplied parameters. */
	if (sc == NULL)
		return 0;

	/*
	 * The suggested minimum timeout value for a controller start operation:
	 *
	 *     Signature FIS Timeout
	 *   + Phy Start Timeout
	 *   + Number of Phy Spin Up Intervals
	 *   ---------------------------------
	 *   Number of milliseconds for the controller start operation.
	 *
	 * NOTE: The number of phy spin up intervals will be equivalent
	 *       to the number of phys divided by the number phys allowed
	 *       per interval - 1 (once OEM parameters are supported).
	 *       Currently we assume only 1 phy per interval. */

	return SCIC_SDS_SIGNATURE_FIS_TIMEOUT
		+ SCIC_SDS_CONTROLLER_PHY_START_TIMEOUT
		+ ((SCI_MAX_PHYS - 1) * SCIC_SDS_CONTROLLER_POWER_CONTROL_INTERVAL);
}

/**
 * scic_controller_stop() - This method will stop an individual controller
 *    object.This method will invoke the associated user callback upon
 *    completion.  The completion callback is called when the following
 *    conditions are met: -# the method return status is SCI_SUCCESS. -# the
 *    controller has been quiesced. This method will ensure that all IO
 *    requests are quiesced, phys are stopped, and all additional operation by
 *    the hardware is halted.
 * @controller: the handle to the controller object to stop.
 * @timeout: This parameter specifies the number of milliseconds in which the
 *    stop operation should complete.
 *
 * The controller must be in the STARTED or STOPPED state. Indicate if the
 * controller stop method succeeded or failed in some way. SCI_SUCCESS if the
 * stop operation successfully began. SCI_WARNING_ALREADY_IN_STATE if the
 * controller is already in the STOPPED state. SCI_FAILURE_INVALID_STATE if the
 * controller is not either in the STARTED or STOPPED states.
 */
enum sci_status scic_controller_stop(
	struct scic_sds_controller *scic,
	u32 timeout)
{
	if (scic->state_machine.current_state_id !=
	    SCI_BASE_CONTROLLER_STATE_READY) {
		dev_warn(scic_to_dev(scic),
			 "SCIC Controller stop operation requested in "
			 "invalid state\n");
		return SCI_FAILURE_INVALID_STATE;
	}

	isci_timer_start(scic->timeout_timer, timeout);
	sci_base_state_machine_change_state(&scic->state_machine,
					    SCI_BASE_CONTROLLER_STATE_STOPPING);
	return SCI_SUCCESS;
}

/**
 * scic_controller_reset() - This method will reset the supplied core
 *    controller regardless of the state of said controller.  This operation is
 *    considered destructive.  In other words, all current operations are wiped
 *    out.  No IO completions for outstanding devices occur.  Outstanding IO
 *    requests are not aborted or completed at the actual remote device.
 * @controller: the handle to the controller object to reset.
 *
 * Indicate if the controller reset method succeeded or failed in some way.
 * SCI_SUCCESS if the reset operation successfully started. SCI_FATAL_ERROR if
 * the controller reset operation is unable to complete.
 */
enum sci_status scic_controller_reset(
	struct scic_sds_controller *scic)
{
	switch (scic->state_machine.current_state_id) {
	case SCI_BASE_CONTROLLER_STATE_RESET:
	case SCI_BASE_CONTROLLER_STATE_READY:
	case SCI_BASE_CONTROLLER_STATE_STOPPED:
	case SCI_BASE_CONTROLLER_STATE_FAILED:
		/*
		 * The reset operation is not a graceful cleanup, just
		 * perform the state transition.
		 */
		sci_base_state_machine_change_state(&scic->state_machine,
				SCI_BASE_CONTROLLER_STATE_RESETTING);
		return SCI_SUCCESS;
	default:
		dev_warn(scic_to_dev(scic),
			 "SCIC Controller reset operation requested in "
			 "invalid state\n");
		return SCI_FAILURE_INVALID_STATE;
	}
}

/**
 * scic_controller_start_io() - This method is called by the SCI user to
 *    send/start an IO request. If the method invocation is successful, then
 *    the IO request has been queued to the hardware for processing.
 * @controller: the handle to the controller object for which to start an IO
 *    request.
 * @remote_device: the handle to the remote device object for which to start an
 *    IO request.
 * @io_request: the handle to the io request object to start.
 * @io_tag: This parameter specifies a previously allocated IO tag that the
 *    user desires to be utilized for this request. This parameter is optional.
 *     The user is allowed to supply SCI_CONTROLLER_INVALID_IO_TAG as the value
 *    for this parameter.
 *
 * - IO tags are a protected resource.  It is incumbent upon the SCI Core user
 * to ensure that each of the methods that may allocate or free available IO
 * tags are handled in a mutually exclusive manner.  This method is one of said
 * methods requiring proper critical code section protection (e.g. semaphore,
 * spin-lock, etc.). - For SATA, the user is required to manage NCQ tags.  As a
 * result, it is expected the user will have set the NCQ tag field in the host
 * to device register FIS prior to calling this method.  There is also a
 * requirement for the user to call scic_stp_io_set_ncq_tag() prior to invoking
 * the scic_controller_start_io() method. scic_controller_allocate_tag() for
 * more information on allocating a tag. Indicate if the controller
 * successfully started the IO request. SCI_SUCCESS if the IO request was
 * successfully started. Determine the failure situations and return values.
 */
enum sci_status scic_controller_start_io(
	struct scic_sds_controller *scic,
	struct scic_sds_remote_device *rdev,
	struct scic_sds_request *req,
	u16 io_tag)
{
	enum sci_status status;

	if (scic->state_machine.current_state_id !=
	    SCI_BASE_CONTROLLER_STATE_READY) {
		dev_warn(scic_to_dev(scic), "invalid state to start I/O");
		return SCI_FAILURE_INVALID_STATE;
	}

	status = scic_sds_remote_device_start_io(scic, rdev, req);
	if (status != SCI_SUCCESS)
		return status;

	scic->io_request_table[scic_sds_io_tag_get_index(req->io_tag)] = req;
	scic_sds_controller_post_request(scic, scic_sds_request_get_post_context(req));
	return SCI_SUCCESS;
}

/**
 * scic_controller_terminate_request() - This method is called by the SCI Core
 *    user to terminate an ongoing (i.e. started) core IO request.  This does
 *    not abort the IO request at the target, but rather removes the IO request
 *    from the host controller.
 * @controller: the handle to the controller object for which to terminate a
 *    request.
 * @remote_device: the handle to the remote device object for which to
 *    terminate a request.
 * @request: the handle to the io or task management request object to
 *    terminate.
 *
 * Indicate if the controller successfully began the terminate process for the
 * IO request. SCI_SUCCESS if the terminate process was successfully started
 * for the request. Determine the failure situations and return values.
 */
enum sci_status scic_controller_terminate_request(
	struct scic_sds_controller *scic,
	struct scic_sds_remote_device *rdev,
	struct scic_sds_request *req)
{
	enum sci_status status;

	if (scic->state_machine.current_state_id !=
	    SCI_BASE_CONTROLLER_STATE_READY) {
		dev_warn(scic_to_dev(scic),
			 "invalid state to terminate request\n");
		return SCI_FAILURE_INVALID_STATE;
	}

	status = scic_sds_io_request_terminate(req);
	if (status != SCI_SUCCESS)
		return status;

	/*
	 * Utilize the original post context command and or in the POST_TC_ABORT
	 * request sub-type.
	 */
	scic_sds_controller_post_request(scic,
		scic_sds_request_get_post_context(req) |
		SCU_CONTEXT_COMMAND_REQUEST_POST_TC_ABORT);
	return SCI_SUCCESS;
}

/**
 * scic_controller_complete_io() - This method will perform core specific
 *    completion operations for an IO request.  After this method is invoked,
 *    the user should consider the IO request as invalid until it is properly
 *    reused (i.e. re-constructed).
 * @controller: The handle to the controller object for which to complete the
 *    IO request.
 * @remote_device: The handle to the remote device object for which to complete
 *    the IO request.
 * @io_request: the handle to the io request object to complete.
 *
 * - IO tags are a protected resource.  It is incumbent upon the SCI Core user
 * to ensure that each of the methods that may allocate or free available IO
 * tags are handled in a mutually exclusive manner.  This method is one of said
 * methods requiring proper critical code section protection (e.g. semaphore,
 * spin-lock, etc.). - If the IO tag for a request was allocated, by the SCI
 * Core user, using the scic_controller_allocate_io_tag() method, then it is
 * the responsibility of the caller to invoke the scic_controller_free_io_tag()
 * method to free the tag (i.e. this method will not free the IO tag). Indicate
 * if the controller successfully completed the IO request. SCI_SUCCESS if the
 * completion process was successful.
 */
enum sci_status scic_controller_complete_io(
	struct scic_sds_controller *scic,
	struct scic_sds_remote_device *rdev,
	struct scic_sds_request *request)
{
	enum sci_status status;
	u16 index;

	switch (scic->state_machine.current_state_id) {
	case SCI_BASE_CONTROLLER_STATE_STOPPING:
		/* XXX: Implement this function */
		return SCI_FAILURE;
	case SCI_BASE_CONTROLLER_STATE_READY:
		status = scic_sds_remote_device_complete_io(scic, rdev, request);
		if (status != SCI_SUCCESS)
			return status;

		index = scic_sds_io_tag_get_index(request->io_tag);
		scic->io_request_table[index] = NULL;
		return SCI_SUCCESS;
	default:
		dev_warn(scic_to_dev(scic), "invalid state to complete I/O");
		return SCI_FAILURE_INVALID_STATE;
	}

}

enum sci_status scic_controller_continue_io(struct scic_sds_request *sci_req)
{
	struct scic_sds_controller *scic = sci_req->owning_controller;

	if (scic->state_machine.current_state_id !=
	    SCI_BASE_CONTROLLER_STATE_READY) {
		dev_warn(scic_to_dev(scic), "invalid state to continue I/O");
		return SCI_FAILURE_INVALID_STATE;
	}

	scic->io_request_table[scic_sds_io_tag_get_index(sci_req->io_tag)] = sci_req;
	scic_sds_controller_post_request(scic, scic_sds_request_get_post_context(sci_req));
	return SCI_SUCCESS;
}

/**
 * scic_controller_start_task() - This method is called by the SCIC user to
 *    send/start a framework task management request.
 * @controller: the handle to the controller object for which to start the task
 *    management request.
 * @remote_device: the handle to the remote device object for which to start
 *    the task management request.
 * @task_request: the handle to the task request object to start.
 * @io_tag: This parameter specifies a previously allocated IO tag that the
 *    user desires to be utilized for this request.  Note this not the io_tag
 *    of the request being managed.  It is to be utilized for the task request
 *    itself. This parameter is optional.  The user is allowed to supply
 *    SCI_CONTROLLER_INVALID_IO_TAG as the value for this parameter.
 *
 * - IO tags are a protected resource.  It is incumbent upon the SCI Core user
 * to ensure that each of the methods that may allocate or free available IO
 * tags are handled in a mutually exclusive manner.  This method is one of said
 * methods requiring proper critical code section protection (e.g. semaphore,
 * spin-lock, etc.). - The user must synchronize this task with completion
 * queue processing.  If they are not synchronized then it is possible for the
 * io requests that are being managed by the task request can complete before
 * starting the task request. scic_controller_allocate_tag() for more
 * information on allocating a tag. Indicate if the controller successfully
 * started the IO request. SCI_TASK_SUCCESS if the task request was
 * successfully started. SCI_TASK_FAILURE_REQUIRES_SCSI_ABORT This value is
 * returned if there is/are task(s) outstanding that require termination or
 * completion before this request can succeed.
 */
enum sci_task_status scic_controller_start_task(
	struct scic_sds_controller *scic,
	struct scic_sds_remote_device *rdev,
	struct scic_sds_request *req,
	u16 task_tag)
{
	enum sci_status status;

	if (scic->state_machine.current_state_id !=
	    SCI_BASE_CONTROLLER_STATE_READY) {
		dev_warn(scic_to_dev(scic),
			 "%s: SCIC Controller starting task from invalid "
			 "state\n",
			 __func__);
		return SCI_TASK_FAILURE_INVALID_STATE;
	}

	status = scic_sds_remote_device_start_task(scic, rdev, req);
	switch (status) {
	case SCI_FAILURE_RESET_DEVICE_PARTIAL_SUCCESS:
		scic->io_request_table[scic_sds_io_tag_get_index(req->io_tag)] = req;

		/*
		 * We will let framework know this task request started successfully,
		 * although core is still woring on starting the request (to post tc when
		 * RNC is resumed.)
		 */
		return SCI_SUCCESS;
	case SCI_SUCCESS:
		scic->io_request_table[scic_sds_io_tag_get_index(req->io_tag)] = req;

		scic_sds_controller_post_request(scic,
			scic_sds_request_get_post_context(req));
		break;
	default:
		break;
	}

	return status;
}

/**
 * scic_controller_get_port_handle() - This method simply provides the user
 *    with a unique handle for a given SAS/SATA core port index.
 * @controller: This parameter represents the handle to the controller object
 *    from which to retrieve a port (SAS or SATA) handle.
 * @port_index: This parameter specifies the port index in the controller for
 *    which to retrieve the port handle. 0 <= port_index < maximum number of
 *    phys.
 * @port_handle: This parameter specifies the retrieved port handle to be
 *    provided to the caller.
 *
 * Indicate if the retrieval of the port handle was successful. SCI_SUCCESS
 * This value is returned if the retrieval was successful.
 * SCI_FAILURE_INVALID_PORT This value is returned if the supplied port id is
 * not in the supported range.
 */
enum sci_status scic_controller_get_port_handle(
	struct scic_sds_controller *scic,
	u8 port_index,
	struct scic_sds_port **port_handle)
{
	if (port_index < scic->logical_port_entries) {
		*port_handle = &scic->port_table[port_index];

		return SCI_SUCCESS;
	}

	return SCI_FAILURE_INVALID_PORT;
}

/**
 * scic_controller_get_phy_handle() - This method simply provides the user with
 *    a unique handle for a given SAS/SATA phy index/identifier.
 * @controller: This parameter represents the handle to the controller object
 *    from which to retrieve a phy (SAS or SATA) handle.
 * @phy_index: This parameter specifies the phy index in the controller for
 *    which to retrieve the phy handle. 0 <= phy_index < maximum number of phys.
 * @phy_handle: This parameter specifies the retrieved phy handle to be
 *    provided to the caller.
 *
 * Indicate if the retrieval of the phy handle was successful. SCI_SUCCESS This
 * value is returned if the retrieval was successful. SCI_FAILURE_INVALID_PHY
 * This value is returned if the supplied phy id is not in the supported range.
 */
enum sci_status scic_controller_get_phy_handle(
	struct scic_sds_controller *scic,
	u8 phy_index,
	struct scic_sds_phy **phy_handle)
{
	if (phy_index < ARRAY_SIZE(scic->phy_table)) {
		*phy_handle = &scic->phy_table[phy_index];

		return SCI_SUCCESS;
	}

	dev_err(scic_to_dev(scic),
		"%s: Controller:0x%p PhyId:0x%x invalid phy index\n",
		__func__, scic, phy_index);

	return SCI_FAILURE_INVALID_PHY;
}

/**
 * scic_controller_allocate_io_tag() - This method will allocate a tag from the
 *    pool of free IO tags. Direct allocation of IO tags by the SCI Core user
 *    is optional. The scic_controller_start_io() method will allocate an IO
 *    tag if this method is not utilized and the tag is not supplied to the IO
 *    construct routine.  Direct allocation of IO tags may provide additional
 *    performance improvements in environments capable of supporting this usage
 *    model.  Additionally, direct allocation of IO tags also provides
 *    additional flexibility to the SCI Core user.  Specifically, the user may
 *    retain IO tags across the lives of multiple IO requests.
 * @controller: the handle to the controller object for which to allocate the
 *    tag.
 *
 * IO tags are a protected resource.  It is incumbent upon the SCI Core user to
 * ensure that each of the methods that may allocate or free available IO tags
 * are handled in a mutually exclusive manner.  This method is one of said
 * methods requiring proper critical code section protection (e.g. semaphore,
 * spin-lock, etc.). An unsigned integer representing an available IO tag.
 * SCI_CONTROLLER_INVALID_IO_TAG This value is returned if there are no
 * currently available tags to be allocated. All return other values indicate a
 * legitimate tag.
 */
u16 scic_controller_allocate_io_tag(
	struct scic_sds_controller *scic)
{
	u16 task_context;
	u16 sequence_count;

	if (!sci_pool_empty(scic->tci_pool)) {
		sci_pool_get(scic->tci_pool, task_context);

		sequence_count = scic->io_request_sequence[task_context];

		return scic_sds_io_tag_construct(sequence_count, task_context);
	}

	return SCI_CONTROLLER_INVALID_IO_TAG;
}

/**
 * scic_controller_free_io_tag() - This method will free an IO tag to the pool
 *    of free IO tags. This method provides the SCI Core user more flexibility
 *    with regards to IO tags.  The user may desire to keep an IO tag after an
 *    IO request has completed, because they plan on re-using the tag for a
 *    subsequent IO request.  This method is only legal if the tag was
 *    allocated via scic_controller_allocate_io_tag().
 * @controller: This parameter specifies the handle to the controller object
 *    for which to free/return the tag.
 * @io_tag: This parameter represents the tag to be freed to the pool of
 *    available tags.
 *
 * - IO tags are a protected resource.  It is incumbent upon the SCI Core user
 * to ensure that each of the methods that may allocate or free available IO
 * tags are handled in a mutually exclusive manner.  This method is one of said
 * methods requiring proper critical code section protection (e.g. semaphore,
 * spin-lock, etc.). - If the IO tag for a request was allocated, by the SCI
 * Core user, using the scic_controller_allocate_io_tag() method, then it is
 * the responsibility of the caller to invoke this method to free the tag. This
 * method returns an indication of whether the tag was successfully put back
 * (freed) to the pool of available tags. SCI_SUCCESS This return value
 * indicates the tag was successfully placed into the pool of available IO
 * tags. SCI_FAILURE_INVALID_IO_TAG This value is returned if the supplied tag
 * is not a valid IO tag value.
 */
enum sci_status scic_controller_free_io_tag(
	struct scic_sds_controller *scic,
	u16 io_tag)
{
	u16 sequence;
	u16 index;

	BUG_ON(io_tag == SCI_CONTROLLER_INVALID_IO_TAG);

	sequence = scic_sds_io_tag_get_sequence(io_tag);
	index    = scic_sds_io_tag_get_index(io_tag);

	if (!sci_pool_full(scic->tci_pool)) {
		if (sequence == scic->io_request_sequence[index]) {
			scic_sds_io_sequence_increment(
				scic->io_request_sequence[index]);

			sci_pool_put(scic->tci_pool, index);

			return SCI_SUCCESS;
		}
	}

	return SCI_FAILURE_INVALID_IO_TAG;
}

void scic_controller_enable_interrupts(
	struct scic_sds_controller *scic)
{
	BUG_ON(scic->smu_registers == NULL);
	writel(0, &scic->smu_registers->interrupt_mask);
}

void scic_controller_disable_interrupts(
	struct scic_sds_controller *scic)
{
	BUG_ON(scic->smu_registers == NULL);
	writel(0xffffffff, &scic->smu_registers->interrupt_mask);
}

static enum sci_status scic_controller_set_mode(
	struct scic_sds_controller *scic,
	enum sci_controller_mode operating_mode)
{
	enum sci_status status          = SCI_SUCCESS;

	if ((scic->state_machine.current_state_id ==
				SCI_BASE_CONTROLLER_STATE_INITIALIZING) ||
	    (scic->state_machine.current_state_id ==
				SCI_BASE_CONTROLLER_STATE_INITIALIZED)) {
		switch (operating_mode) {
		case SCI_MODE_SPEED:
			scic->remote_node_entries      = SCI_MAX_REMOTE_DEVICES;
			scic->task_context_entries     = SCU_IO_REQUEST_COUNT;
			scic->uf_control.buffers.count =
				SCU_UNSOLICITED_FRAME_COUNT;
			scic->completion_event_entries = SCU_EVENT_COUNT;
			scic->completion_queue_entries =
				SCU_COMPLETION_QUEUE_COUNT;
			break;

		case SCI_MODE_SIZE:
			scic->remote_node_entries      = SCI_MIN_REMOTE_DEVICES;
			scic->task_context_entries     = SCI_MIN_IO_REQUESTS;
			scic->uf_control.buffers.count =
				SCU_MIN_UNSOLICITED_FRAMES;
			scic->completion_event_entries = SCU_MIN_EVENTS;
			scic->completion_queue_entries =
				SCU_MIN_COMPLETION_QUEUE_ENTRIES;
			break;

		default:
			status = SCI_FAILURE_INVALID_PARAMETER_VALUE;
			break;
		}
	} else
		status = SCI_FAILURE_INVALID_STATE;

	return status;
}

/**
 * scic_sds_controller_reset_hardware() -
 *
 * This method will reset the controller hardware.
 */
static void scic_sds_controller_reset_hardware(
	struct scic_sds_controller *scic)
{
	/* Disable interrupts so we dont take any spurious interrupts */
	scic_controller_disable_interrupts(scic);

	/* Reset the SCU */
	writel(0xFFFFFFFF, &scic->smu_registers->soft_reset_control);

	/* Delay for 1ms to before clearing the CQP and UFQPR. */
	udelay(1000);

	/* The write to the CQGR clears the CQP */
	writel(0x00000000, &scic->smu_registers->completion_queue_get);

	/* The write to the UFQGP clears the UFQPR */
	writel(0, &scic->scu_registers->sdma.unsolicited_frame_get_pointer);
}

enum sci_status scic_user_parameters_set(
	struct scic_sds_controller *scic,
	union scic_user_parameters *scic_parms)
{
	u32 state = scic->state_machine.current_state_id;

	if (state == SCI_BASE_CONTROLLER_STATE_RESET ||
	    state == SCI_BASE_CONTROLLER_STATE_INITIALIZING ||
	    state == SCI_BASE_CONTROLLER_STATE_INITIALIZED) {
		u16 index;

		/*
		 * Validate the user parameters.  If they are not legal, then
		 * return a failure.
		 */
		for (index = 0; index < SCI_MAX_PHYS; index++) {
			struct sci_phy_user_params *user_phy;

			user_phy = &scic_parms->sds1.phys[index];

			if (!((user_phy->max_speed_generation <=
						SCIC_SDS_PARM_MAX_SPEED) &&
			      (user_phy->max_speed_generation >
						SCIC_SDS_PARM_NO_SPEED)))
				return SCI_FAILURE_INVALID_PARAMETER_VALUE;

			if (user_phy->in_connection_align_insertion_frequency <
					3)
				return SCI_FAILURE_INVALID_PARAMETER_VALUE;

			if ((user_phy->in_connection_align_insertion_frequency <
						3) ||
			    (user_phy->align_insertion_frequency == 0) ||
			    (user_phy->
				notify_enable_spin_up_insertion_frequency ==
						0))
				return SCI_FAILURE_INVALID_PARAMETER_VALUE;
		}

		if ((scic_parms->sds1.stp_inactivity_timeout == 0) ||
		    (scic_parms->sds1.ssp_inactivity_timeout == 0) ||
		    (scic_parms->sds1.stp_max_occupancy_timeout == 0) ||
		    (scic_parms->sds1.ssp_max_occupancy_timeout == 0) ||
		    (scic_parms->sds1.no_outbound_task_timeout == 0))
			return SCI_FAILURE_INVALID_PARAMETER_VALUE;

		memcpy(&scic->user_parameters, scic_parms, sizeof(*scic_parms));

		return SCI_SUCCESS;
	}

	return SCI_FAILURE_INVALID_STATE;
}

int scic_oem_parameters_validate(struct scic_sds_oem_params *oem)
{
	int i;

	for (i = 0; i < SCI_MAX_PORTS; i++)
		if (oem->ports[i].phy_mask > SCIC_SDS_PARM_PHY_MASK_MAX)
			return -EINVAL;

	for (i = 0; i < SCI_MAX_PHYS; i++)
		if (oem->phys[i].sas_address.high == 0 &&
		    oem->phys[i].sas_address.low == 0)
			return -EINVAL;

	if (oem->controller.mode_type == SCIC_PORT_AUTOMATIC_CONFIGURATION_MODE) {
		for (i = 0; i < SCI_MAX_PHYS; i++)
			if (oem->ports[i].phy_mask != 0)
				return -EINVAL;
	} else if (oem->controller.mode_type == SCIC_PORT_MANUAL_CONFIGURATION_MODE) {
		u8 phy_mask = 0;

		for (i = 0; i < SCI_MAX_PHYS; i++)
			phy_mask |= oem->ports[i].phy_mask;

		if (phy_mask == 0)
			return -EINVAL;
	} else
		return -EINVAL;

	if (oem->controller.max_concurrent_dev_spin_up > MAX_CONCURRENT_DEVICE_SPIN_UP_COUNT)
		return -EINVAL;

	return 0;
}

enum sci_status scic_oem_parameters_set(struct scic_sds_controller *scic,
					union scic_oem_parameters *scic_parms)
{
	u32 state = scic->state_machine.current_state_id;

	if (state == SCI_BASE_CONTROLLER_STATE_RESET ||
	    state == SCI_BASE_CONTROLLER_STATE_INITIALIZING ||
	    state == SCI_BASE_CONTROLLER_STATE_INITIALIZED) {

		if (scic_oem_parameters_validate(&scic_parms->sds1))
			return SCI_FAILURE_INVALID_PARAMETER_VALUE;
		scic->oem_parameters.sds1 = scic_parms->sds1;

		return SCI_SUCCESS;
	}

	return SCI_FAILURE_INVALID_STATE;
}

void scic_oem_parameters_get(
	struct scic_sds_controller *scic,
	union scic_oem_parameters *scic_parms)
{
	memcpy(scic_parms, (&scic->oem_parameters), sizeof(*scic_parms));
}

#define INTERRUPT_COALESCE_TIMEOUT_BASE_RANGE_LOWER_BOUND_NS 853
#define INTERRUPT_COALESCE_TIMEOUT_BASE_RANGE_UPPER_BOUND_NS 1280
#define INTERRUPT_COALESCE_TIMEOUT_MAX_US                    2700000
#define INTERRUPT_COALESCE_NUMBER_MAX                        256
#define INTERRUPT_COALESCE_TIMEOUT_ENCODE_MIN                7
#define INTERRUPT_COALESCE_TIMEOUT_ENCODE_MAX                28

/**
 * scic_controller_set_interrupt_coalescence() - This method allows the user to
 *    configure the interrupt coalescence.
 * @controller: This parameter represents the handle to the controller object
 *    for which its interrupt coalesce register is overridden.
 * @coalesce_number: Used to control the number of entries in the Completion
 *    Queue before an interrupt is generated. If the number of entries exceed
 *    this number, an interrupt will be generated. The valid range of the input
 *    is [0, 256]. A setting of 0 results in coalescing being disabled.
 * @coalesce_timeout: Timeout value in microseconds. The valid range of the
 *    input is [0, 2700000] . A setting of 0 is allowed and results in no
 *    interrupt coalescing timeout.
 *
 * Indicate if the user successfully set the interrupt coalesce parameters.
 * SCI_SUCCESS The user successfully updated the interrutp coalescence.
 * SCI_FAILURE_INVALID_PARAMETER_VALUE The user input value is out of range.
 */
static enum sci_status scic_controller_set_interrupt_coalescence(
	struct scic_sds_controller *scic_controller,
	u32 coalesce_number,
	u32 coalesce_timeout)
{
	u8 timeout_encode = 0;
	u32 min = 0;
	u32 max = 0;

	/* Check if the input parameters fall in the range. */
	if (coalesce_number > INTERRUPT_COALESCE_NUMBER_MAX)
		return SCI_FAILURE_INVALID_PARAMETER_VALUE;

	/*
	 *  Defined encoding for interrupt coalescing timeout:
	 *              Value   Min      Max     Units
	 *              -----   ---      ---     -----
	 *              0       -        -       Disabled
	 *              1       13.3     20.0    ns
	 *              2       26.7     40.0
	 *              3       53.3     80.0
	 *              4       106.7    160.0
	 *              5       213.3    320.0
	 *              6       426.7    640.0
	 *              7       853.3    1280.0
	 *              8       1.7      2.6     us
	 *              9       3.4      5.1
	 *              10      6.8      10.2
	 *              11      13.7     20.5
	 *              12      27.3     41.0
	 *              13      54.6     81.9
	 *              14      109.2    163.8
	 *              15      218.5    327.7
	 *              16      436.9    655.4
	 *              17      873.8    1310.7
	 *              18      1.7      2.6     ms
	 *              19      3.5      5.2
	 *              20      7.0      10.5
	 *              21      14.0     21.0
	 *              22      28.0     41.9
	 *              23      55.9     83.9
	 *              24      111.8    167.8
	 *              25      223.7    335.5
	 *              26      447.4    671.1
	 *              27      894.8    1342.2
	 *              28      1.8      2.7     s
	 *              Others Undefined */

	/*
	 * Use the table above to decide the encode of interrupt coalescing timeout
	 * value for register writing. */
	if (coalesce_timeout == 0)
		timeout_encode = 0;
	else{
		/* make the timeout value in unit of (10 ns). */
		coalesce_timeout = coalesce_timeout * 100;
		min = INTERRUPT_COALESCE_TIMEOUT_BASE_RANGE_LOWER_BOUND_NS / 10;
		max = INTERRUPT_COALESCE_TIMEOUT_BASE_RANGE_UPPER_BOUND_NS / 10;

		/* get the encode of timeout for register writing. */
		for (timeout_encode = INTERRUPT_COALESCE_TIMEOUT_ENCODE_MIN;
		      timeout_encode <= INTERRUPT_COALESCE_TIMEOUT_ENCODE_MAX;
		      timeout_encode++) {
			if (min <= coalesce_timeout &&  max > coalesce_timeout)
				break;
			else if (coalesce_timeout >= max && coalesce_timeout < min * 2
				 && coalesce_timeout <= INTERRUPT_COALESCE_TIMEOUT_MAX_US * 100) {
				if ((coalesce_timeout - max) < (2 * min - coalesce_timeout))
					break;
				else{
					timeout_encode++;
					break;
				}
			} else {
				max = max * 2;
				min = min * 2;
			}
		}

		if (timeout_encode == INTERRUPT_COALESCE_TIMEOUT_ENCODE_MAX + 1)
			/* the value is out of range. */
			return SCI_FAILURE_INVALID_PARAMETER_VALUE;
	}

	writel(SMU_ICC_GEN_VAL(NUMBER, coalesce_number) |
	       SMU_ICC_GEN_VAL(TIMER, timeout_encode),
	       &scic_controller->smu_registers->interrupt_coalesce_control);


	scic_controller->interrupt_coalesce_number = (u16)coalesce_number;
	scic_controller->interrupt_coalesce_timeout = coalesce_timeout / 100;

	return SCI_SUCCESS;
}


struct scic_sds_controller *scic_controller_alloc(struct device *dev)
{
	return devm_kzalloc(dev, sizeof(struct scic_sds_controller), GFP_KERNEL);
}

enum sci_status scic_controller_initialize(
	struct scic_sds_controller *scic)
{
	struct sci_base_state_machine *sm = &scic->state_machine;
	enum sci_status result = SCI_SUCCESS;
	struct isci_host *ihost;
	u32 index, state;

	if (scic->state_machine.current_state_id !=
	    SCI_BASE_CONTROLLER_STATE_RESET) {
		dev_warn(scic_to_dev(scic),
			 "SCIC Controller initialize operation requested "
			 "in invalid state\n");
		return SCI_FAILURE_INVALID_STATE;
	}


	ihost = sci_object_get_association(scic);

	sci_base_state_machine_change_state(sm, SCI_BASE_CONTROLLER_STATE_INITIALIZING);

	scic->timeout_timer = isci_timer_create(ihost,
						scic,
						scic_sds_controller_timeout_handler);

	scic_sds_controller_initialize_phy_startup(scic);

	scic_sds_controller_initialize_power_control(scic);

	/*
	 * There is nothing to do here for B0 since we do not have to
	 * program the AFE registers.
	 * / @todo The AFE settings are supposed to be correct for the B0 but
	 * /       presently they seem to be wrong. */
	scic_sds_controller_afe_initialization(scic);

	if (result == SCI_SUCCESS) {
		u32 status;
		u32 terminate_loop;

		/* Take the hardware out of reset */
		writel(0, &scic->smu_registers->soft_reset_control);

		/*
		 * / @todo Provide meaningfull error code for hardware failure
		 * result = SCI_FAILURE_CONTROLLER_HARDWARE; */
		result = SCI_FAILURE;
		terminate_loop = 100;

		while (terminate_loop-- && (result != SCI_SUCCESS)) {
			/* Loop until the hardware reports success */
			udelay(SCU_CONTEXT_RAM_INIT_STALL_TIME);
			status = readl(&scic->smu_registers->control_status);

			if ((status & SCU_RAM_INIT_COMPLETED) ==
					SCU_RAM_INIT_COMPLETED)
				result = SCI_SUCCESS;
		}
	}

	if (result == SCI_SUCCESS) {
		u32 max_supported_ports;
		u32 max_supported_devices;
		u32 max_supported_io_requests;
		u32 device_context_capacity;

		/*
		 * Determine what are the actaul device capacities that the
		 * hardware will support */
		device_context_capacity =
			readl(&scic->smu_registers->device_context_capacity);


		max_supported_ports = smu_dcc_get_max_ports(device_context_capacity);
		max_supported_devices = smu_dcc_get_max_remote_node_context(device_context_capacity);
		max_supported_io_requests = smu_dcc_get_max_task_context(device_context_capacity);

		/*
		 * Make all PEs that are unassigned match up with the
		 * logical ports
		 */
		for (index = 0; index < max_supported_ports; index++) {
			struct scu_port_task_scheduler_group_registers __iomem
				*ptsg = &scic->scu_registers->peg0.ptsg;

			writel(index, &ptsg->protocol_engine[index]);
		}

		/* Record the smaller of the two capacity values */
		scic->logical_port_entries =
			min(max_supported_ports, scic->logical_port_entries);

		scic->task_context_entries =
			min(max_supported_io_requests,
			    scic->task_context_entries);

		scic->remote_node_entries =
			min(max_supported_devices, scic->remote_node_entries);

		/*
		 * Now that we have the correct hardware reported minimum values
		 * build the MDL for the controller.  Default to a performance
		 * configuration.
		 */
		scic_controller_set_mode(scic, SCI_MODE_SPEED);
	}

	/* Initialize hardware PCI Relaxed ordering in DMA engines */
	if (result == SCI_SUCCESS) {
		u32 dma_configuration;

		/* Configure the payload DMA */
		dma_configuration =
			readl(&scic->scu_registers->sdma.pdma_configuration);
		dma_configuration |=
			SCU_PDMACR_GEN_BIT(PCI_RELAXED_ORDERING_ENABLE);
		writel(dma_configuration,
			&scic->scu_registers->sdma.pdma_configuration);

		/* Configure the control DMA */
		dma_configuration =
			readl(&scic->scu_registers->sdma.cdma_configuration);
		dma_configuration |=
			SCU_CDMACR_GEN_BIT(PCI_RELAXED_ORDERING_ENABLE);
		writel(dma_configuration,
			&scic->scu_registers->sdma.cdma_configuration);
	}

	/*
	 * Initialize the PHYs before the PORTs because the PHY registers
	 * are accessed during the port initialization.
	 */
	if (result == SCI_SUCCESS) {
		/* Initialize the phys */
		for (index = 0;
		     (result == SCI_SUCCESS) && (index < SCI_MAX_PHYS);
		     index++) {
			result = scic_sds_phy_initialize(
				&scic->phy_table[index],
				&scic->scu_registers->peg0.pe[index].tl,
				&scic->scu_registers->peg0.pe[index].ll);
		}
	}

	if (result == SCI_SUCCESS) {
		/* Initialize the logical ports */
		for (index = 0;
		     (index < scic->logical_port_entries) &&
		     (result == SCI_SUCCESS);
		     index++) {
			result = scic_sds_port_initialize(
				&scic->port_table[index],
				&scic->scu_registers->peg0.ptsg.port[index],
				&scic->scu_registers->peg0.ptsg.protocol_engine,
				&scic->scu_registers->peg0.viit[index]);
		}
	}

	if (result == SCI_SUCCESS)
		result = scic_sds_port_configuration_agent_initialize(
				scic,
				&scic->port_agent);

	/* Advance the controller state machine */
	if (result == SCI_SUCCESS)
		state = SCI_BASE_CONTROLLER_STATE_INITIALIZED;
	else
		state = SCI_BASE_CONTROLLER_STATE_FAILED;
	sci_base_state_machine_change_state(sm, state);

	return result;
}

enum sci_status scic_controller_start(struct scic_sds_controller *scic,
		u32 timeout)
{
	enum sci_status result;
	u16 index;

	if (scic->state_machine.current_state_id !=
	    SCI_BASE_CONTROLLER_STATE_INITIALIZED) {
		dev_warn(scic_to_dev(scic),
			 "SCIC Controller start operation requested in "
			 "invalid state\n");
		return SCI_FAILURE_INVALID_STATE;
	}

	/* Build the TCi free pool */
	sci_pool_initialize(scic->tci_pool);
	for (index = 0; index < scic->task_context_entries; index++)
		sci_pool_put(scic->tci_pool, index);

	/* Build the RNi free pool */
	scic_sds_remote_node_table_initialize(
			&scic->available_remote_nodes,
			scic->remote_node_entries);

	/*
	 * Before anything else lets make sure we will not be
	 * interrupted by the hardware.
	 */
	scic_controller_disable_interrupts(scic);

	/* Enable the port task scheduler */
	scic_sds_controller_enable_port_task_scheduler(scic);

	/* Assign all the task entries to scic physical function */
	scic_sds_controller_assign_task_entries(scic);

	/* Now initialze the completion queue */
	scic_sds_controller_initialize_completion_queue(scic);

	/* Initialize the unsolicited frame queue for use */
	scic_sds_controller_initialize_unsolicited_frame_queue(scic);

	/* Start all of the ports on this controller */
	for (index = 0; index < scic->logical_port_entries; index++) {
		struct scic_sds_port *sci_port = &scic->port_table[index];

		result = sci_port->state_handlers->start_handler(
				sci_port);
		if (result)
			return result;
	}

	scic_sds_controller_start_next_phy(scic);

	isci_timer_start(scic->timeout_timer, timeout);

	sci_base_state_machine_change_state(&scic->state_machine,
					    SCI_BASE_CONTROLLER_STATE_STARTING);

	return SCI_SUCCESS;
}

/**
 *
 * @object: This is the object which is cast to a struct scic_sds_controller
 *    object.
 *
 * This method implements the actions taken by the struct scic_sds_controller on entry
 * to the SCI_BASE_CONTROLLER_STATE_INITIAL. - Set the state handlers to the
 * controllers initial state. none This function should initialze the
 * controller object.
 */
static void scic_sds_controller_initial_state_enter(void *object)
{
	struct scic_sds_controller *scic;

	scic = (struct scic_sds_controller *)object;

	sci_base_state_machine_change_state(&scic->state_machine,
			SCI_BASE_CONTROLLER_STATE_RESET);
}

/**
 *
 * @object: This is the object which is cast to a struct scic_sds_controller
 *    object.
 *
 * This method implements the actions taken by the struct scic_sds_controller on exit
 * from the SCI_BASE_CONTROLLER_STATE_STARTING. - This function stops the
 * controller starting timeout timer. none
 */
static inline void scic_sds_controller_starting_state_exit(void *object)
{
	struct scic_sds_controller *scic = (struct scic_sds_controller *)object;

	isci_timer_stop(scic->timeout_timer);
}

/**
 *
 * @object: This is the object which is cast to a struct scic_sds_controller
 *    object.
 *
 * This method implements the actions taken by the struct scic_sds_controller on entry
 * to the SCI_BASE_CONTROLLER_STATE_READY. - Set the state handlers to the
 * controllers ready state. none
 */
static void scic_sds_controller_ready_state_enter(void *object)
{
	struct scic_sds_controller *scic;

	scic = (struct scic_sds_controller *)object;

	/* set the default interrupt coalescence number and timeout value. */
	scic_controller_set_interrupt_coalescence(
		scic, 0x10, 250);
}

/**
 *
 * @object: This is the object which is cast to a struct scic_sds_controller
 *    object.
 *
 * This method implements the actions taken by the struct scic_sds_controller on exit
 * from the SCI_BASE_CONTROLLER_STATE_READY. - This function does nothing. none
 */
static void scic_sds_controller_ready_state_exit(void *object)
{
	struct scic_sds_controller *scic;

	scic = (struct scic_sds_controller *)object;

	/* disable interrupt coalescence. */
	scic_controller_set_interrupt_coalescence(scic, 0, 0);
}

/**
 *
 * @object: This is the object which is cast to a struct scic_sds_controller
 *    object.
 *
 * This method implements the actions taken by the struct scic_sds_controller on entry
 * to the SCI_BASE_CONTROLLER_STATE_READY. - Set the state handlers to the
 * controllers ready state. - Stop the phys on this controller - Stop the ports
 * on this controller - Stop all of the remote devices on this controller none
 */
static void scic_sds_controller_stopping_state_enter(void *object)
{
	struct scic_sds_controller *scic;

	scic = (struct scic_sds_controller *)object;

	/* Stop all of the components for this controller */
	scic_sds_controller_stop_phys(scic);
	scic_sds_controller_stop_ports(scic);
	scic_sds_controller_stop_devices(scic);
}

/**
 *
 * @object: This is the object which is cast to a struct
 * scic_sds_controller object.
 *
 * This funciton implements the actions taken by the struct scic_sds_controller
 * on exit from the SCI_BASE_CONTROLLER_STATE_STOPPING. -
 * This function stops the controller stopping timeout timer.
 */
static inline void scic_sds_controller_stopping_state_exit(void *object)
{
	struct scic_sds_controller *scic =
		(struct scic_sds_controller *)object;

	isci_timer_stop(scic->timeout_timer);
}

static void scic_sds_controller_resetting_state_enter(void *object)
{
	struct scic_sds_controller *scic;

	scic = container_of(object, typeof(*scic), parent);
	scic_sds_controller_reset_hardware(scic);
	sci_base_state_machine_change_state(&scic->state_machine,
					    SCI_BASE_CONTROLLER_STATE_RESET);
}

static const struct sci_base_state scic_sds_controller_state_table[] = {
	[SCI_BASE_CONTROLLER_STATE_INITIAL] = {
		.enter_state = scic_sds_controller_initial_state_enter,
	},
	[SCI_BASE_CONTROLLER_STATE_RESET] = {},
	[SCI_BASE_CONTROLLER_STATE_INITIALIZING] = {},
	[SCI_BASE_CONTROLLER_STATE_INITIALIZED] = {},
	[SCI_BASE_CONTROLLER_STATE_STARTING] = {
		.exit_state  = scic_sds_controller_starting_state_exit,
	},
	[SCI_BASE_CONTROLLER_STATE_READY] = {
		.enter_state = scic_sds_controller_ready_state_enter,
		.exit_state  = scic_sds_controller_ready_state_exit,
	},
	[SCI_BASE_CONTROLLER_STATE_RESETTING] = {
		.enter_state = scic_sds_controller_resetting_state_enter,
	},
	[SCI_BASE_CONTROLLER_STATE_STOPPING] = {
		.enter_state = scic_sds_controller_stopping_state_enter,
		.exit_state = scic_sds_controller_stopping_state_exit,
	},
	[SCI_BASE_CONTROLLER_STATE_STOPPED] = {},
	[SCI_BASE_CONTROLLER_STATE_FAILED] = {}
};

/**
 * scic_controller_construct() - This method will attempt to construct a
 *    controller object utilizing the supplied parameter information.
 * @c: This parameter specifies the controller to be constructed.
 * @scu_base: mapped base address of the scu registers
 * @smu_base: mapped base address of the smu registers
 *
 * Indicate if the controller was successfully constructed or if it failed in
 * some way. SCI_SUCCESS This value is returned if the controller was
 * successfully constructed. SCI_WARNING_TIMER_CONFLICT This value is returned
 * if the interrupt coalescence timer may cause SAS compliance issues for SMP
 * Target mode response processing. SCI_FAILURE_UNSUPPORTED_CONTROLLER_TYPE
 * This value is returned if the controller does not support the supplied type.
 * SCI_FAILURE_UNSUPPORTED_INIT_DATA_VERSION This value is returned if the
 * controller does not support the supplied initialization data version.
 */
enum sci_status scic_controller_construct(struct scic_sds_controller *scic,
					  void __iomem *scu_base,
					  void __iomem *smu_base)
{
	u8 i;

	sci_base_state_machine_construct(&scic->state_machine,
		&scic->parent, scic_sds_controller_state_table,
		SCI_BASE_CONTROLLER_STATE_INITIAL);

	sci_base_state_machine_start(&scic->state_machine);

	scic->scu_registers = scu_base;
	scic->smu_registers = smu_base;

	scic_sds_port_configuration_agent_construct(&scic->port_agent);

	/* Construct the ports for this controller */
	for (i = 0; i < SCI_MAX_PORTS; i++)
		scic_sds_port_construct(&scic->port_table[i], i, scic);
	scic_sds_port_construct(&scic->port_table[i], SCIC_SDS_DUMMY_PORT, scic);

	/* Construct the phys for this controller */
	for (i = 0; i < SCI_MAX_PHYS; i++) {
		/* Add all the PHYs to the dummy port */
		scic_sds_phy_construct(&scic->phy_table[i],
				       &scic->port_table[SCI_MAX_PORTS], i);
	}

	scic->invalid_phy_mask = 0;

	/* Set the default maximum values */
	scic->completion_event_entries      = SCU_EVENT_COUNT;
	scic->completion_queue_entries      = SCU_COMPLETION_QUEUE_COUNT;
	scic->remote_node_entries           = SCI_MAX_REMOTE_DEVICES;
	scic->logical_port_entries          = SCI_MAX_PORTS;
	scic->task_context_entries          = SCU_IO_REQUEST_COUNT;
	scic->uf_control.buffers.count      = SCU_UNSOLICITED_FRAME_COUNT;
	scic->uf_control.address_table.count = SCU_UNSOLICITED_FRAME_COUNT;

	/* Initialize the User and OEM parameters to default values. */
	scic_sds_controller_set_default_config_parameters(scic);

	return scic_controller_reset(scic);
}
