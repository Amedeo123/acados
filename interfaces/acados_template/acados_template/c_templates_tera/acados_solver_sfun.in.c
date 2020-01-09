/*
 * Copyright 2019 Gianluca Frison, Dimitris Kouzoupis, Robin Verschueren,
 * Andrea Zanelli, Niels van Duijkeren, Jonathan Frey, Tommaso Sartor,
 * Branimir Novoselnik, Rien Quirynen, Rezart Qelibari, Dang Doan,
 * Jonas Koenemann, Yutao Chen, Tobias Schöls, Jonas Schlagenhauf, Moritz Diehl
 *
 * This file is part of acados.
 *
 * The 2-Clause BSD License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.;
 */

#define S_FUNCTION_NAME   acados_solver_sfunction_{{ model.name }}
#define S_FUNCTION_LEVEL  2

#define MDL_START

// acados
#include "acados/utils/print.h"
#include "acados_c/sim_interface.h"
#include "acados_c/external_function_interface.h"

// example specific
#include "{{ model.name }}_model/{{ model.name }}_model.h"
#include "acados_solver_{{ model.name }}.h"

#include "simstruc.h"

#define SAMPLINGTIME -1

// ** global data **
ocp_nlp_in * nlp_in;
ocp_nlp_out * nlp_out;
ocp_nlp_solver * nlp_solver;
void * nlp_opts;
ocp_nlp_plan * nlp_solver_plan;
ocp_nlp_config * nlp_config;
ocp_nlp_dims * nlp_dims;

{%- if solver_options.integrator_type == 'ERK' -%}
external_function_param_casadi * forw_vde_casadi;
{%- if solver_options.hessian_approx == 'EXACT' %} 
external_function_param_casadi * hess_vde_casadi;
{% endif %}
{% elif solver_options.integrator_type == 'IRK' %}
external_function_param_casadi * impl_dae_fun;
external_function_param_casadi * impl_dae_fun_jac_x_xdot_z;
external_function_param_casadi * impl_dae_jac_x_xdot_u_z;
{% endif %}

{%- if constraints.constr_type == "BGP" %}
// external_function_param_casadi * r_constraint;
external_function_param_casadi * phi_constraint;
{%- endif %}
{%- if constraints.constr_type_e == "BGP" -%}
// external_function_param_casadi r_e_constraint;
external_function_param_casadi phi_e_constraint;
{%- endif %}
{%- if constraints.constr_type == "BGH" %}
external_function_param_casadi * h_constraint;
{%- endif %}
{%- if constraints.constr_type_e == "BGH" %}
external_function_param_casadi h_e_constraint;
{%- endif %}


static void mdlInitializeSizes (SimStruct *S)
{
    // specify the number of continuous and discrete states
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);

    {# compute number of input ports #}
    {%- set n_inputs = 1 %}  {# x0 #}
    {%- if dims.np > 0 %}  {# parameters #}
        {%- set n_inputs = n_inputs + 1 -%}
    {%- endif %}
    {%- if dims.ny > 0 %}  {# y_ref -#}
        {%- set n_inputs = n_inputs + 1 -%}
    {%- endif %}
    {%- if dims.ny_e > 0 %}  {# y_ref_e #}
        {%- set n_inputs = n_inputs + 1 %}
    {%- endif %}
    {%- if dims.nbx > 0 %}  {# lbx, ubx #}
        {%- set n_inputs = n_inputs + 2 %}
    {%- endif %}
    {%- if dims.nbu > 0 %}  {# lbu, ubu #}
        {%- set n_inputs = n_inputs + 2 %}
    {%- endif %}
    {%- if dims.ng > 0 %}  {# lg, ug #}
        {%- set n_inputs = n_inputs + 2 %}
    {%- endif %}
    {%- if dims.nh > 0 %}  {# lh, uh #}
        {%- set n_inputs = n_inputs + 2 %}
    {%- endif %}

    // specify the number of input ports
    if ( !ssSetNumInputPorts(S, {{ n_inputs }}) )
        return;

    // specify the number of output ports
    if ( !ssSetNumOutputPorts(S, 5) )
        return;

    // specify dimension information for the input ports
    {%- set i_input = 0 %}
    // x0
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.nx }});
    {%- if dims.ny > 0 %}
    {%- set i_input = i_input + 1 %}
    // y_ref
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.ny }});
    {%- endif %}

    {%- if dims.ny_e > 0 %}
    {%- set i_input = i_input + 1 %}
    // y_ref_e
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.ny_e }});
    {%- endif %}

    {%- if dims.np > 0 %}
    {%- set i_input = i_input + 1 %}
    // parameters
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.np }});
    {%- endif %}

    {%- if dims.nbx > 0 %}
    {%- set i_input = i_input + 1 %}
    // lbx
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.nbx }});
    {%- set i_input = i_input + 1 %}
    // ubx
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.nbx }});
    {%- endif %}

    {%- if dims.nbu > 0 %}
    {%- set i_input = i_input + 1 %}
    // lbu
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.nbu }});
    {%- set i_input = i_input + 1 %}
    // ubu
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.nbu }});
    {%- endif %}

    {%- if dims.ng > 0 %}
    {%- set i_input = i_input + 1 %}
    // lg
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.ng }});
    {%- set i_input = i_input + 1 %}
    // ug
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.ng }});
    {%- endif %}

    {%- if dims.nh > 0 %}
    {%- set i_input = i_input + 1 %}
    // lh
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.nh }});
    {%- set i_input = i_input + 1 %}
    // uh
    ssSetInputPortVectorDimension(S, {{ i_input }}, {{ dims.nh }});
    {%- endif %}

    // specify dimension information for the output ports
    ssSetOutputPortVectorDimension(S, 0, {{ dims.nu }} ); // optimal input
    ssSetOutputPortVectorDimension(S, 1, 1 ); // solver status
    ssSetOutputPortVectorDimension(S, 2, 1 ); // KKT residuals
    ssSetOutputPortVectorDimension(S, 3, {{ dims.nx }} ); // first state
    ssSetOutputPortVectorDimension(S, 4, 1); // computation times

    // specify the direct feedthrough status
    // should be set to 1 for all inputs used in mdlOutputs
    {%- for i in range(end=n_inputs) %}
    ssSetInputPortDirectFeedThrough(S, {{ i }}, 1);
    {%- endfor %}

    // one sample time
    ssSetNumSampleTimes(S, 1);
}


#if defined(MATLAB_MEX_FILE)

#define MDL_SET_INPUT_PORT_DIMENSION_INFO
#define MDL_SET_OUTPUT_PORT_DIMENSION_INFO

static void mdlSetInputPortDimensionInfo(SimStruct *S, int_T port, const DimsInfo_T *dimsInfo)
{
    if ( !ssSetInputPortDimensionInfo(S, port, dimsInfo) )
         return;
}

static void mdlSetOutputPortDimensionInfo(SimStruct *S, int_T port, const DimsInfo_T *dimsInfo)
{
    if ( !ssSetOutputPortDimensionInfo(S, port, dimsInfo) )
         return;
}

#endif /* MATLAB_MEX_FILE */


static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, SAMPLINGTIME);
    ssSetOffsetTime(S, 0, 0.0);
}


static void mdlStart(SimStruct *S)
{
    acados_create();
}

static void mdlOutputs(SimStruct *S, int_T tid)
{
    InputRealPtrsType in_sign;
    {% set input_sizes = [dims.nx, dims.ny, dims.ny_e, dims.np, dims.nbx, dims.nbu, dims.ng, dims.nh] %}

    // local buffer
    {%- set buffer_size =  input_sizes | sort | last %}
    real_t buffer[{{ buffer_size }}];


    /* go through inputs */
    {%- set i_input = 0 %}
    // initial condition
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});
    for (int i = 0; i < {{ dims.nx }}; i++)
        buffer[i] = (double)(*in_sign[i]);
    ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, 0, "lbx", buffer);
    ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, 0, "ubx", buffer);

{% if dims.ny > 0 %}
    // y_ref
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.ny }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    for (int ii = 0; ii < {{ dims.N }}; ii++)
        ocp_nlp_cost_model_set(nlp_config, nlp_dims, nlp_in, ii, "yref", (void *) buffer);
{%- endif %}


{% if dims.ny_e > 0 %}
    // y_ref_e
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.ny_e }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    ocp_nlp_cost_model_set(nlp_config, nlp_dims, nlp_in, {{ dims.N }}, "yref", (void *) buffer);
{%- endif %}

// TODO this is only for stage-invariant parameters !!!!!!!!!!!!!!!
{% if dims.np > 0 %}
    // parameters
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.np }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    // update value of parameters
    for (int ii = 0; ii <= {{ dims.N }}; ii++) 
        acados_update_params(ii, buffer, {{ dims.np }});
{%- endif %}


{% if dims.nbx > 0 %}
    // lbx
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.nbx }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    for (int ii = 1; ii < {{ dims.N }}; ii++)
        ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, ii, "lbx", buffer);

    // ubx
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.nbx }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    for (int ii = 1; ii < {{ dims.N }}; ii++)
        ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, ii, "ubx", buffer);
{%- endif %}

{% if dims.nbu > 0 %}
    // lbu
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.nbu }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    for (int ii = 0; ii < {{ dims.N }}; ii++)
        ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, ii, "lbu", buffer);

    // ubu
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.nbu }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    for (int ii = 0; ii < {{ dims.N }}; ii++)
        ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, ii, "ubu", buffer);
{%- endif %}

{% if dims.ng > 0 %}
    // lg
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.ng }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    for (int ii = 0; ii < {{ dims.N }}; ii++)
        ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, ii, "lg", buffer);

    // ug
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.ng }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    for (int ii = 0; ii < {{ dims.N }}; ii++)
        ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, ii, "ug", buffer);
{%- endif %}

{% if dims.nh > 0 %}
    // lh
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.nh }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    for (int ii = 0; ii < {{ dims.N }}; ii++)
        ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, ii, "lh", buffer);

    // uh
    {%- set i_input = i_input + 1 %}
    in_sign = ssGetInputPortRealSignalPtrs(S, {{ i_input }});

    for (int i = 0; i < {{ dims.nh }}; i++)
        buffer[i] = (double)(*in_sign[i]);

    for (int ii = 0; ii < {{ dims.N }}; ii++)
        ocp_nlp_constraints_model_set(nlp_config, nlp_dims, nlp_in, ii, "uh", buffer);
{%- endif %}

    /* call solver */
    int acados_status = acados_solve();


    /* set outputs */
    // assign pointers to output signals
    real_t *out_u0, *out_status, *out_KKT_res, *out_x1, *out_cpu_time;

    out_u0          = ssGetOutputPortRealSignal(S, 0);
    out_status      = ssGetOutputPortRealSignal(S, 1);
    out_KKT_res     = ssGetOutputPortRealSignal(S, 2);
    out_x1          = ssGetOutputPortRealSignal(S, 3);
    out_cpu_time    = ssGetOutputPortRealSignal(S, 4);

    // extract solver info
    *out_status = (real_t) acados_status;
    *out_KKT_res = (real_t) nlp_out->inf_norm_res;
    *out_cpu_time = (real_t) nlp_out->total_time;
    
    // get solution
    ocp_nlp_out_get(nlp_config, nlp_dims, nlp_out, 0, "u", (void *) out_u0);

    // get next state
    ocp_nlp_out_get(nlp_config, nlp_dims, nlp_out, 1, "x", (void *) out_x1);

}

static void mdlTerminate(SimStruct *S)
{
    acados_free();
}


#ifdef  MATLAB_MEX_FILE
#include "simulink.c"
#else
#include "cg_sfun.h"
#endif
