<div align="center">
  <h1>Pintos 프로젝트</h1>
  <h2>KAIST 대학교 실습용 OS 구현 프로젝트</h2>
  <h4>🗝️ KeyWords <h4/>
  <p> #pintos #Thread #UserProgram #VirtualMemory #OperatingSystem </p>
  <p> #Synchronization #SystemCall #Engineering </p>
  <br>
  <div align="center">
    <img src="https://img.shields.io/badge/C-00599C?style=flat-square&logo=C&logoColor=white"/>
    <img src="https://img.shields.io/badge/gcc-00599C?style=flat-square&logo=gcc&logoColor=white"/>
    <img src="https://img.shields.io/badge/AWS%20EC2-FF9900?style=flat-square&logo=amazon-aws&logoColor=white"/>
    <img src="https://img.shields.io/badge/Linux-FCC624?style=flat-square&logo=linux&logoColor=black"/>
  </div>
  <br>
</div>

---

# 개요
Pintos 프로젝트는 KAIST 실습용 운영체제(OS) 구현 프로젝트입니다.   
Thread, User Program, Virtual Memory를 구현하며 OS에 대한 이해를 증진시키고 엔지니어링 실력을 향상시키기 위한 목적입니다.

# 목적
- 운영체제 시스템에 대한 심화된 이해
- 동시성 문제 및 메모리 관리 기술 습득
- 효율적인 자원 관리 및 성능 개선 경험

# 기능 구현

### ⚙️ Thread 시스템
- 최소한의 기능을 제공하는 스레드 시스템을 확장하여 동기화 문제를 더 잘 이해하는 것을 목표로 구현했습니다. 
- 주 작업 디렉토리는 `threads`입니다.
- **주요 기능**:
  - 스레드 간의 동기화 처리
  - 여러 스레드가 동시에 실행되는 환경 구현
- **구현 성과**
<details>
- <summary> mlfqs 제외한 priority donation (20 pass / 7 fail) </summary>
- <img width="192" alt="image" src="https://github.com/user-attachments/assets/99d10aa4-12dd-41b6-87af-f0506089d58d">
</details>




### 📝 User Program
- Pintos의 인프라와 스레드 패키지에 익숙해진 후, 사용자 프로그램을 실행할 수 있는 시스템을 구현합니다. 기본 코드는 이미 사용자 프로그램을 로드하고 실행하는 기능을 제공하지만, I/O 또는 상호작용은 불가능합니다. 이 프로젝트에서는 시스템 콜을 통해 프로그램이 운영체제와 상호작용할 수 있도록 구현했습니다.
- **주요 기능**:
  - 시스템 호출을 통해 프로그램과 OS 간의 인터랙션 구현
  - 사용자 프로그램의 메모리 관리
- **구현 성과**
<details>
- <summary> 일부 케이스(rox, bad) 제외한 userprogram (79 pass / 95 fail)</summary>
- <img width="190" alt="image" src="https://github.com/user-attachments/assets/e13632f5-0809-4e72-99cd-d7cef12a070b">
</details>


### 🧠 Virtual Memory
- 이제 Pintos의 내부 구조와 스레드 처리에 익숙해졌다면, 가상 메모리 시스템을 구현할 차례입니다. 기존에는 메모리 크기에 제한이 있었으나, 이번 프로젝트를 통해 다중 사용자 프로그램을 동시에 실행할 수 있도록 가상 메모리 관리 시스템을 구현하였습니다.
- **주요 기능**:
  - 메모리 페이징 처리
  - 메인 메모리 크기 이상의 프로그램 실행 가능
- **구현 성과**
<details>
- <summary> 일부 케이스(rox, bad) 제외한 userprogram (87 pass / 54 fail)</summary>
- <img width="190" alt="image" src="https://github.com/user-attachments/assets/140f6e88-262d-4eca-b05a-bd0725984723">
</details>

# 🛠️ 엔지니어링 경험
<details>
<summary> 1. 헤더 중복 및 순환 참조 문제 해결 </summary>
- 헤더 파일의 순환 참조 문제 발생, 코드 리팩토링을 통한 해결       

- <img width="1080" alt="image" src="https://github.com/user-attachments/assets/382ae70e-02d4-475e-a4b6-4375a1d42f27">

</details>

<details>
<summary> 2. 스레드 스케줄링 성능 개선 </summary>
- 싱글 스레드 스케줄러 성능 개선(kernel threads 850 -> 310)       

- <img width="440" alt="image" src="https://github.com/user-attachments/assets/fd2d3d2f-f0af-4d93-8d7a-de2e6b9a8cb5">

</details>

<details>
<summary>3. 메모리 할당기 최적화</summary>
- 정책 결정을 통한 메모리 내부 단편화 문제 해결 및 성능 개선 (42/100 -> 84/100)    

  - <img width="440" alt="image" src="https://github.com/user-attachments/assets/32e802fb-8a51-4729-b588-5c8e377fe64d">
</details>

# 🔎 개발환경

개발 환경 설정은 다음과 같습니다.

# [.env]
