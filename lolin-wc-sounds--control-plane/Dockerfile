FROM lolin-wc-sounds-cp-base:latest

COPY wc_control/ /app/wc_control/
COPY static/ /app/static/
COPY run.py /app/

WORKDIR /app/

EXPOSE 8338

CMD ["python3", "-m", "wc_control"]
