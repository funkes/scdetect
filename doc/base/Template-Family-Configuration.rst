.. _template-family-configuration-label:

Template family configuration
=============================

The template family configuration defines a group of templates which are usually
generated by a cluster analysis approach. The members of a template family
define which *reference magnitudes* are part of a regression for a certain group
of templates.

The template family configuration must be provided as a JSON configuration file
which includes a JSON array of *template family configuration objects*. An
exemplary template family configuration file containing the definition for a
single template family may look like:

.. code-block:: json

   [
     {
       "id": "e7bc36fc-95e4-45de-a33d-6dcef43ca809",
       "references": [
         {
           "detectorId": "detector-01",
           "streams": [
             {
               "templateWaveformId": "NET.STA00.LOC.HHZ",
               "upperLimit": 2
             }
           ]
         },
         {
           "detectorId": "detector-02",
           "lowerLimit": 0.5,
           "upperLimit": 2.5,
           "streams": [
             {
               "templateWaveformId": "NET.STA01.LOC.HHZ"
             },
             {
               "templateWaveformId": "NET.STA02.LOC.BHE",
               "upperLimit": 2.3
             }
           ]
         },
         {
           "originId": "originPublicId",
           "streams": [
             {
               "templatePhase": "Pg",
               "templateWaveformId": "NET.STA01.LOC.HHZ",
               "templateWaveformStart": -2,
               "templateWaveformEnd": 2
             }
           ]
         }
       ]
     }
   ]

A template family configuration object may have the following attributes:


* 
  ``"id"``\ : The optional template family identifier. Currently, this identifier
  has no special meaning except of the fact that it is used in log messages. If
  no value is specified a unique identifier is created automatically.

* 
  ``"references"``\ : Required. A JSON array of template family members
  configuration objects. Members may reference detectors (defined by
  the :ref:`template configuration <template-configuration-label>`) or third-party
  origins (i.e. origins which are not used for detections but contribute to the
  amplitude-magnitude regression).

Detector reference configuration
--------------------------------

The so-called *detector reference* members reference detector configurations
from the :ref:`template configuration <template-configuration-label>`. The reference is
established by means of the ``"detectorId"`` attribute. Detector reference members
imply that


* 
  reference magnitudes are added to the template family. That is, based both on
  the origin identifier (\ ``"originId"``\ ) and the sensor locations (defined by
  corresponding streams (\ ``"streams"``\ )).

* 
  for the sensor locations referenced new magnitudes will be computed in case of
  detections issued by the corresponding detector.

Detector reference configuration objects allow the following attributes to be
specified:


* 
  ``"detectorId"``\ : Required. Detector identifier used for establishing a
  reference to a detector configuration from
  the :ref:`template configuration <template-configuration-label>`.

* 
  ``"streams"``\ : Required. A JSON array of so-called *sensor location
  configuration objects*. Again, in the context of a detector reference, a
  sensor location configuration object defines the attributes:


  * 
    ``"templateWaveformId"``\ : Required. The template waveform identifier
    regarding a detector configuration stream configuration. Required, in
    order to fully establish the relation detector - (template)
    :external:term:`origin` - sensor location.

    Usually, this refers to
    a `FDSN Source Identifier <http://docs.fdsn.org/projects/source-identifiers/en/v1.0/>`_
    . Note that the part relevant for specifying a sensor location is taken
    into account, only.

  * 
    ``"lowerLimit"``\ : The optional lower limit for magnitudes estimated.
    Magnitudes smaller than the limit specified won't be issued.

  * 
    ``"upperLimit"``\ : The optional upper limit for magnitudes estimated.
    Magnitudes greater than the limit specified won't be issued.

.. _detector-reference-configuration-defaults-label:

Detector reference configuration defaults
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following sensor location configuration defaults may be defined within the
scope of a detector reference configuration:


* ``"lowerLimit"``
* ``"upperLimit"``

That is, if not explicitly overridden within sensor location configurations the
corresponding fallback values will be used, instead.

.. _third-party-reference-configuration-label:

Third-party reference configuration
-----------------------------------

The so-called *third-party reference* members reference origins which are not
used for detection, but they contribute to the amplitude-magnitude regression. A
third-party reference configuration object allows the following attributes to be
defined:


* 
  ``"originId"``\ : Required. The origin identifier used to establish a
  reference to an :external:term:`origin` in the catalog. Usually, the origin
  identifier corresponds to a *seismic metadata resource identifier* (``smi``).

* 
  ``"streams"``\ : Required. A JSON array of sensor location configuration objects
  (now, in the context of a third-party reference configuration). In the context
  of a third-party reference configuration a sensor location configuration
  allows the following attributes to be specified:


  * 
    ``"templateWaveformId"``\ : Required. The template waveform identifier
    regarding a (template) origin. Required, in order to fully establish the
    relation origin - sensor location.

    Usually, this refers to
    a `FDSN Source Identifier <http://docs.fdsn.org/projects/source-identifiers/en/v1.0/>`_
    . Note that the part relevant for specifying a sensor location taken is
    into account, only.

  * 
    ``"templatePhase"``\ : Required. A string defining the template phase code
    used for amplitude calculation. It is the phase code which actually
    defines the *reference time* used for waveform extraction.

  * 
    ``"templateWaveformStart"``\ : The template waveform start in seconds with
    regard to the reference time. A negative value refers to a waveform
    start *before* the reference time, while a positive value means *after*
    the reference time.

  * 
    ``"templateWaveformEnd"``\ : The waveform end in seconds with regard to the
    reference time. A negative value refers to a waveform start *before* the
    reference time, while a positive value means *after* the reference time.

