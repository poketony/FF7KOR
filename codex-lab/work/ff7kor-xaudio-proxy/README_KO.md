# FF7 2026 XAudio2 proxy probe

stock `FFVII.exe`가 import하는 `XAudio2_9Redist.dll` 프록시입니다.
Steamworks/런처 쪽을 건드리지 않고 2026 본체 안에서 우리 코드가 실행되는지 확인하기 위한 1차 프로브입니다.

아직 폰트 렌더링을 고치지 않습니다. 로그로 다음을 확인합니다.

- 프록시 DLL이 `FFVII.exe`에 로드되는지
- 패치한 `ff7_ja`의 field decode jump 바이트가 메모리에 올라오는지
- 원본 field decode 바이트가 다른 위치에 남아 있는지

## 배치 방법

게임을 완전히 종료한 뒤, 게임 최상단 폴더에서 직접 처리합니다.

```text
C:\Program Files (x86)\Steam\steamapps\common\FINAL FANTASY VII Steam Edition
```

1. 원본 `xaudio2_9redist.dll`을 `xaudio2_9redist_orig.dll`로 이름 변경합니다.
2. 빌드된 프록시 `xaudio2_9redist.dll`을 같은 폴더에 넣습니다.
3. 평소처럼 Steam/런처에서 게임을 실행합니다.
4. 같은 폴더에 생기는 `ff7kor-xaudio-proxy.log`를 확인합니다.

## 기대 로그

```text
ff7kor XAudio2 proxy loaded
probe started, pid=...
probe summary: patched ff7_ja field decode jump hits=...
probe summary: original ff7_ja field decode code hits=...
probe summary: FFVII resources/ff7_1.02 format string hits=...
probe finished
```

`patched ff7_ja field decode jump` hit가 있으면, 수정한 `ff7_ja`는 메모리에 올라오지만 실행 경로가 따로 있다는 뜻입니다.
hit가 없으면 stock 2026 본체가 `ff7_ja` 파일을 다른 방식으로 읽거나 캐시하고 있다는 뜻입니다.

## 복구

1. 프록시 `xaudio2_9redist.dll`을 제거합니다.
2. `xaudio2_9redist_orig.dll` 이름을 다시 `xaudio2_9redist.dll`로 되돌립니다.
