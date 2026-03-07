# CanvasOS v1.0.1-p10-merged

> C 기반 결정론적 타일 실행 시스템 — Phase 6~10 통합 머지 빌드

```
Phase-6: PASS 6/6 · Phase-7: PASS 10/10 · Phase-8: PASS 18/18 · Phase-9: PASS 20/20 · Phase-10: PASS 20/20
```



> 병합 기준
> - Phase 10을 최신 메인라인으로 채택
> - Phase 9 VM/PixelCode 반영
> - Phase 8 userland/proc/syscall 계층 유지
> - Phase 6/7의 portable workers(barrier) 수정본 유지
> - Phase 6 delta 자산은 보존만 하고 현재 메인 빌드 경로에는 미연결

---

## 한 줄 요약

**1024×1024칸 모눈종이(8MB) 위에서 돌아가는 결정론적 운영체제.**
데이터를 나란히 배열하지 않고, Y축(시간축)으로 수직 적층하여 압축 사용합니다.

---

## 핵심 철학: Always-ON

캔버스에는 항상 전기가 흐릅니다.
프로그램을 "켜는" 것이 아니라, **문(Gate)을 열면 실행, 닫으면 차단**입니다.

## 5대 헌법

1. 스캔은 중심 (512,512)에서 시작
2. 좌표 열거는 Ring(MH) — 완전 결정론
3. 기본 = CLOSE, OPEN만 실행
4. ABGR 4채널 계약 불변
5. 동일 입력 = 동일 결과

---

## 빌드 & 실행

```bash
# Linux / macOS
make test_all           # 전체 테스트 (16개)
make sjterm && ./sjterm  # 편집 터미널
make tervas && ./tervas  # 읽기 전용 터미널
make hello_canvas && ./examples/hello_canvas  # 예시

# Android Termux
pkg install clang make
export CC=clang
make sjterm && cp sjterm ~ && ~/sjterm
```

---

## 메모리 구조

```
Cell = { A(32bit), B(8bit), G(8bit), R(8bit), pad(8bit) } = 8 bytes
Canvas = 1024 × 1024 Cells = 8 MB
Tile = 16×16 Cells, 4096 tiles, 각각 Gate(OPEN/CLOSE) 보유
```

| 채널 | 역할 | 비유 |
|------|------|------|
| **A** | 주소/링크 (WHERE) | 교실 번호 |
| **B** | 동작/타입 (WHAT) | 수업 과목 |
| **G** | 상태/에너지 (HOW) | 배터리 잔량 |
| **R** | 데이터 (DATA) | 칠판에 쓴 글자 |

## 4분면 + WH/BH

```
     Q0 (0,0)        Q1 (512,0)
     ┌────────────┬────────────┐
     │            │            │
     │            │            │
     ├────────────┼────────────┤ y=512
     │            │ WH 512×128 │ ← tick 로그 (32768 records)
     │    Q2      │ BH 512×64  │ ← 에너지 감쇠
     │            │ Q3 나머지  │
     └────────────┴────────────┘ y=1023
```

---

## 결정론 규약 (DK-1 ~ DK-5)

| ID | 규칙 | 위반 시 |
|----|------|---------|
| DK-1 | tick 경계에서만 커밋 | 결정론 붕괴 |
| DK-2 | 정수 연산만 (float 금지) | 플랫폼 차이 발생 |
| DK-3 | cell_index 오름차순 merge | 순서 의존 버그 |
| DK-4 | 결과값 clamp (DK_CLAMP_U8) | 오버플로우 |
| DK-5 | ±1 노이즈 흡수 | Lane 간 불일치 |

---

## WH 옵코드 레퍼런스

| Code | 이름 | 설명 |
|------|------|------|
| 0x10 | WH_GATE_OPEN | 타일 게이트 열기 |
| 0x11 | WH_GATE_CLOSE | 타일 게이트 닫기 |
| 0x20 | WH_WRITE | WH 레코드 기록 |
| 0x21 | BH_DECAY | BH 에너지 감쇠 |
| 0x30 | WH_IPC_SEND | IPC 레코드 |
| 0x40 | CVP_SAVE | 캔버스 → .cvp 저장 |
| 0x41 | CVP_LOAD | .cvp → 캔버스 로드 |
| 0x42 | CVP_VALIDATE | .cvp 무결성 검사 |
| 0x43 | CVP_REPLAY | WH 리플레이 |
| 0x50 | ENGCTX_TICK | tick 진행 |
| 0x51 | ENGCTX_REPLAY | 부작용 재실행 |
| 0x52 | ENGCTX_INSPECT | 셀 조회 |

SSOT: `include/canvasos_opcodes.def`

---

## Tervas 터미널 (Phase-7)

읽기 전용(R-4) 운영 인터페이스. 엔진 상태를 **보기만** 합니다.

```
tervas> view all              # 전체 canvas
tervas> view wh               # WhiteHole 영역
tervas> inspect 512 512       # 셀 상세 조회
tervas> quick now              # 현재 tick + refresh
tervas> help                   # 전체 22개 명령 목록
```

**불변 규약**: R-1(Y=시간), R-4(READ-ONLY), R-6(정수만)

---

## SJTerm (편집 터미널)

SJ-PTL 언어로 캔버스를 직접 편집합니다.

```
> :512,512              # 커서 이동
> B=01 G=64 R='A'       # 레지스터 설정
> !                      # 커밋 (셀에 기록, y+1)
> go 100                 # gate 100 열기
> sv                     # CVP 저장
> ?                      # 현재 셀 조회
```

---

## 예시: HELLO 수직 적층

`examples/hello_canvas.c`를 실행하면:

```
[STEP 3] "HELLO" 수직 적층 (x=512, y=512~516)
  > (512,512) A=00010000 B=01 G=100 R=48('H')
  > (512,513) A=00010001 B=01 G=100 R=45('E')
  > (512,514) A=00010002 B=01 G=100 R=4C('L')
  > (512,515) A=00010003 B=01 G=100 R=4C('L')
  > (512,516) A=00010004 B=01 G=100 R=4F('O')
```

같은 X좌표에서 Y축으로 데이터가 쌓입니다.
8MB 안에서 데이터를 수직 적층하여 압축 사용하는 핵심 원리입니다.

---

## 디렉토리 구조

```
CanvasOS/
├── include/           헤더 (타입, 엔진, 결정론, WH/BH, Tervas)
│   └── tervas/        Phase-7 Tervas 헤더 7개
├── src/               소스 (엔진, 스캔, CVP, FS, Tervas)
│   └── tervas/        Phase-7 Tervas 소스 6개
├── tests/             테스트 소스 8개 (Phase 5/6/7)
├── examples/          예시 코드
│   └── hello_canvas.c HELLO 수직 적층 시연
├── docs/              명세서 + 개발사전
│   ├── SPEC_CanvasOS_Phase7_Tervas_v1.1.md
│   ├── ENGINE_CANVASOS_v1.md
│   └── devdict_phase7.html   ← 통합 개발사전 (P4+5+7)
├── scripts/           빌드/배포 스크립트
├── tools/             생성기 도구
├── Makefile
├── VERSION            v1.0.1-p7
└── .gitignore
```

---

## 테스트 (16개)

| Phase | ID | 검증 |
|-------|-----|------|
| P6 | T1 | 입력동일 → hash 동일 |
| P6 | T2 | CVP save/load → hash 동일 |
| P6 | T3 | replay → gate 상태 동일 |
| P6 | T4 | 멀티스레드 hash 동일 |
| P6 | T5 | SJ-PTL → CVP → hash 동일 |
| P6 | T6 | BH-IDLE 압축 + ref 보존 |
| P7 | TV1~TV10 | snapshot, projection, style, dispatch, quick... |

---

## 개발사전

`docs/devdict_phase7.html`을 브라우저에서 열면 검색 가능한 개발사전이 나옵니다.
Phase 4+5+7의 모든 구조체, 함수, 상수, 옵코드, 규약을 검색할 수 있습니다.

---

## Phase 로드맵

| Phase | 상태 | 핵심 |
|-------|------|------|
| 0~4 | LOCKED | 엔진 골격, FS, CVP |
| 5 | LOCKED | 결정론 DK-1~5, Lane, Merge |
| 6 | PASS 6/6 | 결정론 엔진 완성 |
| 7 | PASS 10/10 | Tervas + Projection + Dispatch |
| 다음 | → | NCurses → SDL2 → OpenGL |

---

## 라이선스

CanvasOS Project — SJCanvas (sjpupro-lab)

---

*v1.0.1-p7 · 2026-03-07*
