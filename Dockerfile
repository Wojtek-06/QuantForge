# QuantForge: C++ lab + research UI (local / placement demos)
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j \
    && ctest --test-dir build --output-on-failure --timeout 120

FROM python:3.12-slim AS runtime
WORKDIR /app
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/quantforge /app/bin/quantforge
COPY --from=build /src/build/lob_replay_export /app/bin/lob_replay_export
COPY --from=build /src/configs /app/configs
COPY --from=build /src/web /app/web
COPY python /app/python
RUN pip install --no-cache-dir -r /app/python/requirements.txt
ENV PATH="/app/bin:${PATH}"
ENV PYTHONPATH=/app/python
ENV QUANTFORGE_BIN=/app/bin/quantforge
EXPOSE 8000
CMD ["uvicorn", "quantforge_research.app:app", "--host", "0.0.0.0", "--port", "8000"]
