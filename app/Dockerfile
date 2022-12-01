FROM python:3.11.0-bullseye

RUN apt-get update && apt-get install redis -y

# Prepare python virtual environment
ENV VIRTUAL_ENV=/app/.venv
RUN python -m venv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

COPY . /app
WORKDIR /app


RUN pip install --upgrade pip && pip install --no-cache-dir -r requirements.txt

CMD ["python", "/app/main.py"]