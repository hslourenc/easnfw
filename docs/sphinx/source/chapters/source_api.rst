************************
Firmware API Reference
************************

This chapter is generated from the firmware's public headers by Doxygen and
Breathe during each Sphinx build. API declarations and their contracts should
be documented in the source headers; this chapter intentionally does not
restate those details in prose.

The :doc:`design` chapter describes the architectural role of each layer.
Hardware wiring and IFL operation are described in :doc:`introduction` and
:doc:`interfw_link`.

EASNFW-SENSOR
=============

Application layer
-----------------

.. doxygenfile:: sampling.h
   :project: sensor

.. doxygenfile:: audio_processing.h
   :project: sensor

.. doxygenfile:: storage.h
   :project: sensor

.. doxygenfile:: cloud_link.h
   :project: sensor

Domain layer
------------

.. doxygenfile:: audio_sampling.h
   :project: sensor

.. doxygenfile:: env_sampling.h
   :project: sensor

.. doxygenfile:: audio_algo.h
   :project: sensor

.. doxygenenum:: easnfw_selftest_failure
   :project: sensor

.. doxygenfunction:: selftest__run
   :project: sensor

.. doxygenfunction:: selftest__audio
   :project: sensor

.. doxygenfunction:: selftest__environment
   :project: sensor

.. doxygenfunction:: selftest__interfw_link
   :project: sensor

.. doxygenfunction:: selftest__cloud
   :project: sensor

.. doxygenfunction:: selftest__nvs
   :project: sensor

.. doxygenfunction:: selftest__mass_storage
   :project: sensor

.. doxygenfile:: time_sync.h
   :project: sensor

The SENSOR command transmit and receive API is defined in ``interfw_link.h``:

.. doxygenfunction:: ifl__reset_easnfwcloud_tx
   :project: sensor

.. doxygenfunction:: ifl__ping_tx
   :project: sensor

.. doxygenfunction:: ifl__utc_time_request_tx
   :project: sensor

.. doxygenfunction:: ifl__easnfwcloud_ready_rx
   :project: sensor

.. doxygenfile:: reset.h
   :project: sensor

The SENSOR message-pool and abort-marker operations are declared in
``pipeline.h``:

.. doxygenfunction:: easnfw_pipeline_message_alloc
   :project: sensor

.. doxygenfunction:: easnfw_pipeline_message_free
   :project: sensor

.. doxygenfunction:: easnfw_pipeline_message_is_abort
   :project: sensor

.. doxygenfunction:: easnfw_pipeline_queue_abort
   :project: sensor

.. doxygenfunction:: easnfw_pipeline_abort_track
   :project: sensor

.. doxygenfile:: config.h
   :project: sensor

.. doxygenfile:: status.h
   :project: sensor

Driver and platform layer
-------------------------

.. doxygenfile:: audio_sensor_drv.h
   :project: sensor

.. doxygenfile:: env_sensor_drv.h
   :project: sensor

.. doxygenfile:: mass_storage.h
   :project: sensor

.. doxygenfile:: nvs.h
   :project: sensor

The SENSOR IFL UART operations are declared in ``uart.h``:

.. doxygenfunction:: uart__is_ifl_init
   :project: sensor

.. doxygenfunction:: uart__ifl_init
   :project: sensor

.. doxygenfunction:: uart__ifl_tx
   :project: sensor

.. doxygenfunction:: uart__ifl_rx_handler_set
   :project: sensor

EASNFW-CLOUD
============

Application layer
-----------------

.. doxygenfile:: sensor_link.h
   :project: cloud

.. doxygenfile:: payload_assembler.h
   :project: cloud

.. doxygenfile:: cloud_tx.h
   :project: cloud

.. doxygenfile:: pipeline.h
   :project: cloud

Domain layer
------------

.. doxygenfile:: payload_format.h
   :project: cloud

.. doxygenfile:: retry.h
   :project: cloud

.. doxygenfile:: selftest.h
   :project: cloud

Driver and platform layer
-------------------------

.. doxygenfile:: http_client.h
   :project: cloud

.. doxygenfile:: lte_m.h
   :project: cloud

The CLOUD UART operations are declared in ``uart.h``:

.. doxygenfunction:: easnfw_cloud_uart_init
   :project: cloud

.. doxygenfunction:: easnfw_cloud_uart_tx
   :project: cloud

.. doxygenfunction:: easnfw_cloud_uart_rx_set
   :project: cloud

Shared protocol
===============

The shared wire protocol declarations, including frame layouts, command IDs,
status codes, payload types, and protocol helpers, are documented from their
single source in :doc:`interfw_link`.
