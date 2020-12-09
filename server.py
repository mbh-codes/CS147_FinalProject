from flask import Flask, request, jsonify, render_template, Response
from datetime import datetime
import webbrowser
import time
import random
import json

app = Flask(__name__)
random.seed()

RINGED = []
LOCKED = []
TIME = []
isLocked = True
isRinged = False
test = True

@app.route("/home")
def Home():
        print("ENTERING HOME VIEW")
        if (len(LOCKED) == 0):
                return "HOME SYSTEM HAS NOT BEEN SETUP YET"

        # locked = LOCKED[-1] == '1'
        # ringed = RINGED[-1] == '1'
        # latestTime = TIME[-1]

        # return "HOME \n locked:{} \n ringed: {} \n at: {}  \n LOCKED: {}\n RINGED: {}\n TIME: {}".format(locked,ringed,latestTime,LOCKED,RINGED, TIME)
        return render_template('index.html')

@app.route("/update/")
def update():
        ringed = request.args.get("RINGED")
        locked = request.args.get("LOCKED")
        if (locked != None and ringed != None):
                global isLocked
                global isRinged
                isLocked = locked == '1'
                isRinged = ringed == '1'
                RINGED.append(ringed)
                LOCKED.append(locked)
                now = datetime.now()
                TIME.append(now.strftime("%Y-%m-%d-%H:%M:%S"))
        return "SUCCESS MESSAGE"

@app.route('/door-data')
def door_data():
        def listen_for_data():
                while True:
                        json_data = json.dumps({'time': datetime.now().strftime('%Y-%m-%d %H:%M:%S'), 'isLocked': isLocked, 'isRinged': isRinged, 'locked': LOCKED, 'ringed': RINGED})
                        yield f"data:{json_data}\n\n"
                        time.sleep(10)
        return Response(listen_for_data(), mimetype='text/event-stream')

@app.route('/chart-data')
def chart_data():
        def generate_random_data():
                while True:
                        json_data = json.dumps({'time': datetime.now().strftime('%Y-%m-%d %H:%M:%S'), 'value': random.random() * 100})
                        yield f"data:{json_data}\n\n"
                        time.sleep(1)
        return Response(generate_random_data(), mimetype='text/event-stream')

@app.route('/')
def index():
        return render_template('index.html')

if __name__ == '__main__':
  app.run(debug=True, threaded=True)
