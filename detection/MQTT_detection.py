import torch
import cv2
import matplotlib.pyplot as plt
import time
import paho.mqtt.client as mqtt
import user_secret


ssid     = user_secret.ssid
password = user_secret.password
mqttuser = user_secret.mqttuser
mqttpass = user_secret.mqttpass

total_device_num = 1;

in_list = []
out_list = []
visitorNumber = 0
changeSignal = 0

for i in range(total_device_num):
    in_list.append(0)
    out_list.append(0)


client_id = "RPi4"

mqtt_server = "mqtt.cetools.org"
port = 1884


def on_message(client, userdata, message):
    print("message received " ,str(message.payload.decode("utf-8")))
    print("message topic=",message.topic)
    print("message qos=",message.qos)
    print("message retain flag=",message.retain)

    for i in range(total_device_num):
        if (message.topic == "student/CASA0022/ucfnzc1/in_" + str(i + 1)):
            in_list[i - 1] = int(message.payload.decode("utf-8"))
            print ("receive message from " + message.topic + ": " + str(in_list[i]))
        if (message.topic == "student/CASA0022/ucfnzc1/out_" + str(i + 1)):
            out_list[i - 1] = int(message.payload.decode("utf-8"))
            print ("receive message from " + message.topic + ": " + str(out_list[i]))

def connect_mqtt():
    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)
    # Set Connecting Client ID

    client = mqtt.Client(client_id)
    client.username_pw_set(mqttuser, mqttpass)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(mqtt_server, port)
    client.loop_start()

    subscribe_topic = []

    for i in range(total_device_num):
        subscribe_topic.append("in_" + str(i + 1))
        subscribe_topic.append("out_" + str(i + 1))

    for topic in subscribe_topic:
        client.subscribe("student/CASA0022/ucfnzc1/" + topic)
        print ("student/CASA0022/ucfnzc1/" + topic)

    return client

def _publish(client, topic, msg):
    result = client.publish(topic, msg)
    status = result[0]
    if status == 0:
        print(f"Send `{msg}` to topic `{topic}`")
    else:
        print(f"Failed to send message to topic {topic}")


def publish(client, loc, visitorNumber, changeSignal):
    # print("Publishing message to topic")
    _publish(client, "student/CASA0022/ucfnzc1/visitorNumber", str(visitorNumber))
    _publish(client, "student/CASA0022/ucfnzc1/changeSignal", str(changeSignal))
    _publish(client, "student/CASA0022/ucfnzc1/Sofa", str(loc[0]))
    _publish(client, "student/CASA0022/ucfnzc1/Equipment", str(loc[1]))
    _publish(client, "student/CASA0022/ucfnzc1/Desk", str(loc[2]))


def get_image():
    capture = cv2.VideoCapture(0)
    ret, frame = capture.read()
    # cv2.imwrite("camera_opencv.jpg", frame)
    frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB) #BGR2RGB
    capture.release()
    return frame
    
def get_score(result, location):
    xmin = max(result.xmin, location[0])
    ymin = max(result.ymin, location[1])
    xmax = min(result.xmax, location[2])
    ymax = min(result.ymax, location[3])
    score = ((xmax - xmin) * (ymax - ymin)) / ((result.xmax - result.xmin) * (result.ymax - result.ymin))
    return score


def get_location(result = None):
    locations = [[0, 0, 115, 260],
                [115, 0, 480, 120],
                [0, 260, 115, 480],
                [115, 120, 480, 480],
                [480, 0, 640, 480]]

    score_1 = get_score(result, locations[0])
    score_2 = get_score(result, locations[1])
    score_3 = get_score(result, locations[2]) + get_score(result, locations[3]) + get_score(result, locations[4])

    #print(score_1, score_2, score_3)

    if (score_1 > score_2):
        if (score_1 > score_3):
            return 0
        else: return 2
    elif (score_2 > score_3):
        return 1
    else: return 2


def main():
    # Model
    model = torch.hub.load('ultralytics/yolov5', 'yolov5s')  # or yolov5n - yolov5x6, custom
    model.classes = [0]

    client = connect_mqtt()
        
    while (True):
        #try :
            #client.loop()
            img = get_image()
            
            # Inference
            results = model(img)
                  
            # Results

            results = results.pandas().xyxy[0]
            
            loc = [0, 0, 0]
            
            color = (0, 0, 255)
            
            img_plot = img.copy()
            
            
            for row_index,result in results.iterrows():
                cv2.rectangle(img, (int(result.xmin), int(result.ymin)),
                                    (int(result.xmax), int(result.ymax)),
                                                    color, 2)
                loc[get_location(result)] += 1
            
            cv2.startWindowThread()
            cv2.namedWindow("image", 0)
            cv2.moveWindow("image", 0, -5)
            cv2.resizeWindow("image", 480, 320)

            
            img = cv2.cvtColor(img, cv2.COLOR_RGB2BGR)
            # cv2.imwrite("camera_result.jpg", img)
            img = cv2.resize(img, (480, 320))
            
            visitorNumber = sum(in_list) - sum(out_list)
            changeSignal = 0
            
            if (visitorNumber < int(len(results))):
                visitorNumber = int(len(results))
                changeSignal = 1

            blind = max(0, visitorNumber - len(results))            
                                                            
            print(str(len(results)) + ' person(s) detected!')
            print(str(loc[0]) + ' in Sofa Area.')
            print(str(loc[1]) + ' in Equipment Working Area.')
            print(str(loc[2]) + ' in Classroom Desk Area.')
            print(str(blind) + ' in Blind Spot Area.')

            publish(client, loc, visitorNumber, changeSignal)

            cv2.imshow("image", img)
            cv2.waitKey(5000)

            # time.sleep(5)
            cv2.destroyAllWindows()

            print("==============wait for 5 seconds=================")
        #except Exception as e:
            #print (e)
            #client.loop_stop()
            #break
        

if __name__ == "__main__":
    main()
