******************
Cloud Backend
******************

Scope and Terminology
=====================

Within the firmware, **EASNFW-CLOUD** names the image running on the nRF9151.
This chapter uses **cloud backend** for the Linux server that receives,
persists, and exposes node data. The two components are independent and shall
not be referred to interchangeably in interfaces or source code.

Pilot Deployment
================

The pilot cloud backend will be hosted on a Linux machine in the laboratory.
Its minimum logical services are:

* an HTTPS ingestion API for EASN nodes;
* a relational database for node, record, environmental, processing, and
  delivery metadata;
* object storage for audio, spectrograms, and other large binary artifacts;
* background workers for validation and subsequent scientific processing;
* a dashboard/API for authorized researchers; and
* monitoring, backup, and restore services.

Services should be deployed as reproducible containers, while persistent data
must be stored outside the containers on explicitly managed volumes. Container
configuration, database migrations, and infrastructure documentation shall be
version controlled without committing credentials or private keys.

Network Architecture
====================

Administrative Access
---------------------

Remote researcher access and device ingestion are separate trust boundaries.
Researchers shall reach the laboratory server through a private VPN such as
WireGuard or Tailscale and authenticate to SSH using individual public keys.
Password authentication and direct public exposure of the SSH port should be
disabled. Administrative users shall not share accounts or private keys.

Device Ingestion
----------------

EASN nodes require an Internet-reachable HTTPS endpoint because their LTE-M
connections originate outside the laboratory network. If the laboratory does
not have a stable public address or is behind carrier-grade NAT, a public relay
or reverse tunnel shall terminate or forward HTTPS traffic through an
authenticated tunnel to the laboratory server.

For improved availability, the public relay may durably queue validated
uploads while the laboratory backend is unavailable. It must preserve the
original ``record_id`` and idempotency semantics and return a cloud commit
acknowledgement only after the configured durability boundary has been met.

Storage Model
=============

The initial backend should keep structured metadata and large binary data
separate:

* PostgreSQL, optionally with TimescaleDB, stores record identities,
  timestamps, environmental values, descriptor metadata, delivery state, and
  object references; and
* an S3-compatible object store such as MinIO, or a managed filesystem with
  equivalent integrity controls, stores raw audio, spectrograms, and other
  binary artifacts.

The ingestion API shall validate schema version, payload size, authentication,
checksums, and record completeness before committing a record. ``record_id``
shall be constrained to be unique so retransmission is idempotent.

Backup and Availability
=======================

RAID or mirrored disks improve hardware availability but do not constitute a
backup. The pilot deployment shall maintain:

* the primary server copy;
* a scheduled backup on a separate local device;
* a periodic off-site copy of scientific data and database backups;
* checksums or object-store integrity metadata; and
* a documented and periodically tested restore procedure.

Monitoring shall cover disk capacity, backup age, failed ingestion, database
health, certificate expiration, and the last contact time of every deployed
node.

Open Decisions
==============

* CBOR-over-HTTPS is the initial candidate for structured node uploads; the
  exact endpoint and schema remain to be defined.
* The public-ingress solution depends on the laboratory network and must be
  selected after checking public IP, NAT, firewall, and institutional security
  constraints.
* Retention periods for raw audio, derived artifacts, and delivered on-node
  records must be agreed with the ecological research team.
* The authentication mechanism for nodes and the FOTA/configuration-command
  channel remain to be specified.
