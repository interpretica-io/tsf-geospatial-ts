# Build environment
#
# Ubuntu 24.04: GEOS 3.12 (M coordinates), PROJ 9.4, GDAL 3.8 - the
# versions ta_geospatial is written against.
FROM ubuntu:24.04

ARG user
ARG uid
ARG gid
ARG docker_gid
ENV USER=${user}
ENV UID=${uid}
ENV GID=${gid}
ENV DOCKER_GID=${docker_gid}

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${DOCKER_GID} docker ; fi

RUN if [ ! -d /home/${USER} ] ; then mkdir -p /home/${USER} && chmod -R 777 /home/${USER} ; fi

# TE's own build dependencies, then the suite's:
#  - libgeos-dev, libproj-dev + proj-data, libgdal-dev + gdal-data: the
#    geospatial engine ta_geospatial links on the agent;
#  - libcurl4-openssl-dev: its OGC requests;
#  - libpq-dev, libsqlite3-dev: tsf-sql's drivers
#    (PostGIS is reached through libpq);
#  - cgi-mapserver: a real WMS/WFS server (MapServer CGI) for the
#    services group, served by Python's CGI handler;
#  - docker.io: the PostGIS server runs in a sibling container.
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y \
        autoconf \
        automake \
        bison \
        build-essential \
        ca-certificates \
        ccache \
        cmake \
        curl \
        docker.io \
        file \
        flex \
        gawk \
        git \
        libbsd-dev \
        libjansson-dev \
        libexpat1 \
        libexpat1-dev \
        libcurl4-openssl-dev \
        libglib2.0-dev \
        libnetconf2-dev \
        libncurses6 \
        libncurses-dev \
        libpam0g \
        libpam0g-dev \
        libpcap0.8-dev \
        libpcre2-dev \
        libpopt-dev \
        libreadline-dev \
        libssh-dev \
        libssh2-1-dev \
        libssl-dev \
        libusb-1.0-0 \
        libusb-1.0-0-dev \
        libxml2 \
        libxml2-dev \
        libxslt1-dev \
        libxslt1.1 \
        libpcre3-dev \
        libyaml-dev \
        libtirpc-dev \
        m4 \
        meson \
        openssh-server \
        pkg-config \
        perl \
        python3 \
        python3-pip \
        rsync \
        ssh \
        sudo \
        tshark \
        tcl-dev \
        wget \
        libgeos-dev \
        libproj-dev \
        proj-data \
        proj-bin \
        libgdal-dev \
        gdal-data \
        gdal-bin \
        libpq-dev \
        libsqlite3-dev \
        postgresql-client \
        cgi-mapserver \
        mapserver-bin

# pygeoapi, the OGC reference implementation of OGC API - Features, for
# the services group, in a venv of its own: pip cannot replace the
# Debian-packaged numpy it would upgrade.
RUN apt-get install -y python3-venv && \
    python3 -m venv /opt/pygeoapi && \
    /opt/pygeoapi/bin/pip install --no-cache-dir 'pygeoapi==0.24.0'

# The build tree lives in /tmp/aibuild; a named volume mounted there
# (scripts/run.sh, TE_DOCKER_CACHE) starts as a copy of this directory,
# so it has to exist and be writable by the container user.
RUN mkdir -p /tmp/aibuild && chmod 1777 /tmp/aibuild

RUN echo "${USER} ALL=NOPASSWD: ALL" >> /etc/sudoers

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${GID} ${USER} ; fi

RUN useradd --system \
            --gid ${GID} --gid ${DOCKER_GID} \
            --uid ${UID} \
            --shell /bin/bash \
            ${USER}
RUN usermod -a -G ${DOCKER_GID} ${USER}
RUN usermod -a -G docker ${USER}
