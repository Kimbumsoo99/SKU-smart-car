#얼굴 인식을 위해 필요한 패키지, 소스파일 선언
from imutils.video import VideoStream
from imutils.video import FPS
#등록해둔 사람의 얼굴의 특징 추출 코드
import face_recognition
import imutils
import pickle
import time  #fps 계산 용
import cv2
import sys
#서보모터를 제어gpio
import RPi.GPIO as GPIO

#서보모터 핀 설정
survo_pin = 23
#gpio mode: 출력 단자로 사용함
GPIO.setmode(GPIO.BCM)
GPIO.setup(survo_pin, GPIO.OUT)
#서보모터 각도 제어는 pwm으로 제어(0~200)
pwm = GPIO.PWM(survo_pin, 200)
#초기각도 180도(pwm 5)
angle = 5
pwm.start(angle)

x=0

#인식할 사람 이름 변수에 default값
currentname = "Unknown"

encodingsP = "encodings.pickle"

print("[INFO] loading encodings + face detector...")
#dataset의 사전 훈련된 얼굴 특징 추출
data = pickle.loads(open(encodingsP, "rb").read())

#카메라 모듈 실시간 영상 stream
vs = VideoStream(usePiCamera=True).start()
time.sleep(2.0)

#frame 계산 시작
fps = FPS().start()

#인식 될 때까지 반복
while True:
  #카메라로부터 영상을 frame단위로 가져옴
   frame = vs.read()
   frame = imutils.resize(frame, width=500)
   boxes = face_recognition.face_locations(frame)
   encodings = face_recognition.face_encodings(frame, boxes)
   names = []

   
   for encoding in encodings:
    #data디렉터리 속 사람 얼굴 vs 현재 모듈 속 사람 얼굴 비교
      matches = face_recognition.compare_faces(data["encodings"],encoding)
      name = "Unknown"

      #얼굴 일치 시
      if True in matches:
         matchedIdxs = [i for (i, b) in enumerate(matches) if b]
         counts = {}

         for i in matchedIdxs:
        #name 변수에 디렉터리의 이름을 변수 대입
            name = data["names"][i]
            counts[name] = counts.get(name, 0) + 1
         
         name = max(counts, key=counts.get)
         
         x += 1

         if currentname != name:
            currentname = name
         
            
         else:
            currentname="Unknown"
            
         
      # names리스트에 현재 인식 이름 등록
      names.append(currentname)

   #등록해둔 사람이 감지됐응 경우
      if currentname!="Unknown" and x>3 and name!="Unknown":
         print(name + " is detected")
         print("execute survo")
         #서보모터 각도 조절 왼쪽 90도 -> 오른쪽 90도
         angle = 25
      #모터 제어 후 파이썬 종료
         pwm.start(angle)
         sys.exit()
      

    #등록되지 않은 사람일 경우 다시 반복문 수행
      elif currentname == "Unknown" and x>3:
         print("strager!!!")
         print(currentname)
         

   
   # loop over the recognized faces
   for ((top, right, bottom, left), name) in zip(boxes, names):
      
    #인식된 사람의 이름을 이미지 위에 표시
    #체크박스 형태
      cv2.rectangle(frame, (left, top), (right, bottom),
         (0, 255, 225), 2)
      y = top - 15 if top - 15 > 15 else top + 15  #체크박스에 표시될 문자 좌표
    #FPS 정보 문자열을 영상 프레임에 출력(putText() 함수)
      cv2.putText(frame, name, (left, y), cv2.FONT_HERSHEY_SIMPLEX,
         .8, (0, 255, 255), 2)

  #해당 영상 프레임을 윈도우 창 실시간 스트림 위에 표시
   cv2.imshow("Facial Recognition is Running", frame)
   key = cv2.waitKey(1) & 0xFF

   # 'q' 입력 시 프로그램 강제 종요
   if key == ord("q"):
      break

   # update the FPS counter
   fps.update()

# 타이머 stop && FPS 정보 출력
fps.stop()
print("[INFO] elasped time: {:.2f}".format(fps.elapsed()))
print("[INFO] approx. FPS: {:.2f}".format(fps.fps()))

# do a bit of cleanup
cv2.destroyAllWindows()
vs.stop()
