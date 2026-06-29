FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake ninja-build catch2 \
    && rm -rf /var/lib/apt/lists/*
RUN pip install --no-cache-dir pytest pyelftools

WORKDIR /workspace
