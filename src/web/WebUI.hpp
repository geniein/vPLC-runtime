#ifndef WEB_UI_HPP
#define WEB_UI_HPP

#include <string>

namespace WebUI {

const std::string INDEX_HTML = R"HTML(
<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>vPLC Embedded Web Dashboard</title>
    <!-- Google Fonts: Inter & Outfit -->
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=Outfit:wght@400;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0b0f19;
            --panel-bg: rgba(17, 24, 39, 0.75);
            --border-color: rgba(255, 255, 255, 0.08);
            --text-primary: #f3f4f6;
            --text-secondary: #9ca3af;
            --accent-cyan: #06b6d4;
            --accent-emerald: #10b981;
            --accent-rose: #f43f5e;
            --accent-violet: #8b5cf6;
            --accent-amber: #f59e0b;
            --font-main: 'Inter', sans-serif;
            --font-title: 'Outfit', sans-serif;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: var(--font-main);
            -webkit-font-smoothing: antialiased;
        }

        body {
            background-color: var(--bg-color);
            background-image: 
                radial-gradient(at 0% 0%, rgba(6, 182, 212, 0.1) 0px, transparent 50%),
                radial-gradient(at 100% 100%, rgba(139, 92, 246, 0.1) 0px, transparent 50%);
            background-attachment: fixed;
            color: var(--text-primary);
            min-height: 100vh;
            padding: 2rem;
            display: flex;
            flex-direction: column;
            gap: 2rem;
        }

        /* Scrollbar styling */
        ::-webkit-scrollbar {
            width: 8px;
            height: 8px;
        }
        ::-webkit-scrollbar-track {
            background: rgba(255, 255, 255, 0.02);
        }
        ::-webkit-scrollbar-thumb {
            background: rgba(255, 255, 255, 0.15);
            border-radius: 4px;
        }
        ::-webkit-scrollbar-thumb:hover {
            background: var(--accent-cyan);
        }

        header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 1.5rem 2rem;
            background: var(--panel-bg);
            border: 1px solid var(--border-color);
            border-radius: 16px;
            backdrop-filter: blur(12px);
            box-shadow: 0 4px 30px rgba(0, 0, 0, 0.4);
        }

        .logo-section h1 {
            font-family: var(--font-title);
            font-size: 1.8rem;
            font-weight: 700;
            letter-spacing: -0.025em;
            background: linear-gradient(135deg, #06b6d4 0%, #8b5cf6 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        .logo-section p {
            font-size: 0.8rem;
            color: var(--text-secondary);
            margin-top: 0.2rem;
        }

        .system-status {
            display: flex;
            gap: 1.5rem;
            align-items: center;
        }

        .status-badge {
            display: flex;
            align-items: center;
            gap: 0.5rem;
            padding: 0.5rem 1rem;
            background: rgba(255, 255, 255, 0.04);
            border: 1px solid var(--border-color);
            border-radius: 9999px;
            font-size: 0.85rem;
            font-weight: 500;
        }

        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background-color: var(--accent-emerald);
            box-shadow: 0 0 12px var(--accent-emerald);
            animation: pulse 2s infinite;
        }

        @keyframes pulse {
            0% { transform: scale(0.95); opacity: 0.5; }
            50% { transform: scale(1.1); opacity: 1; box-shadow: 0 0 16px var(--accent-emerald); }
            100% { transform: scale(0.95); opacity: 0.5; }
        }

        .main-container {
            display: grid;
            grid-template-columns: 420px 1fr;
            gap: 2rem;
            align-items: start;
        }

        @media (max-width: 1200px) {
            .main-container {
                grid-template-columns: 1fr;
            }
        }

        .card {
            background: var(--panel-bg);
            border: 1px solid var(--border-color);
            border-radius: 16px;
            backdrop-filter: blur(12px);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            padding: 2rem;
            display: flex;
            flex-direction: column;
            gap: 1.5rem;
            transition: transform 0.3s ease, box-shadow 0.3s ease;
        }

        .card:hover {
            box-shadow: 0 8px 36px rgba(6, 182, 212, 0.05);
        }

        .card-title {
            font-family: var(--font-title);
            font-size: 1.3rem;
            font-weight: 600;
            border-left: 4px solid var(--accent-cyan);
            padding-left: 0.75rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        /* Form styling */
        .form-group {
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
        }

        .form-group label {
            font-size: 0.85rem;
            font-weight: 500;
            color: var(--text-secondary);
        }

        .form-control {
            background: rgba(255, 255, 255, 0.03);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 0.75rem 1rem;
            color: var(--text-primary);
            font-size: 0.9rem;
            outline: none;
            transition: all 0.2s ease;
        }

        .form-control:focus {
            border-color: var(--accent-cyan);
            box-shadow: 0 0 8px rgba(6, 182, 212, 0.25);
            background: rgba(255, 255, 255, 0.06);
        }

        select.form-control {
            appearance: none;
            background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' fill='none' viewBox='0 0 24 24' stroke='%239ca3af'%3E%3Cpath stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M19 9l-7 7-7-7'/%3E%3C/svg%3E");
            background-repeat: no-repeat;
            background-position: right 1rem center;
            background-size: 1.25rem;
            padding-right: 2.5rem;
        }

        /* Preview Addresses styling */
        .preview-box {
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid var(--border-color);
            border-radius: 10px;
            padding: 1rem;
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 0.75rem;
        }

        .preview-item {
            display: flex;
            flex-direction: column;
            gap: 0.25rem;
        }

        .preview-label {
            font-size: 0.7rem;
            color: var(--text-secondary);
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.05em;
        }

        .preview-value {
            font-size: 0.9rem;
            font-family: monospace;
            font-weight: 700;
            color: var(--accent-cyan);
        }

        .btn {
            background: linear-gradient(135deg, var(--accent-cyan) 0%, var(--accent-violet) 100%);
            color: white;
            border: none;
            border-radius: 8px;
            padding: 0.85rem 1.5rem;
            font-size: 0.95rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            box-shadow: 0 4px 12px rgba(6, 182, 212, 0.2);
        }

        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 18px rgba(6, 182, 212, 0.35);
        }

        .btn:active {
            transform: translateY(0);
        }

        /* Tag Table Grid styling */
        .table-wrapper {
            width: 100%;
            overflow-x: auto;
            border-radius: 12px;
            border: 1px solid var(--border-color);
        }

        table {
            width: 100%;
            border-collapse: collapse;
            text-align: left;
            font-size: 0.9rem;
        }

        th {
            background: rgba(255, 255, 255, 0.02);
            color: var(--text-secondary);
            font-weight: 600;
            padding: 1rem 1.25rem;
            border-bottom: 1px solid var(--border-color);
            font-size: 0.8rem;
            text-transform: uppercase;
            letter-spacing: 0.05em;
        }

        td {
            padding: 1rem 1.25rem;
            border-bottom: 1px solid var(--border-color);
            color: var(--text-primary);
        }

        tr:last-child td {
            border-bottom: none;
        }

        tr:hover td {
            background: rgba(255, 255, 255, 0.015);
        }

        .badge {
            display: inline-block;
            padding: 0.25rem 0.6rem;
            border-radius: 4px;
            font-size: 0.75rem;
            font-weight: 600;
        }

        .badge-coils { background: rgba(6, 182, 212, 0.12); color: var(--accent-cyan); border: 1px solid rgba(6, 182, 212, 0.2); }
        .badge-inputs { background: rgba(139, 92, 246, 0.12); color: #a78bfa; border: 1px solid rgba(139, 92, 246, 0.2); }
        .badge-input-regs { background: rgba(245, 158, 11, 0.12); color: #fbbf24; border: 1px solid rgba(245, 158, 11, 0.2); }
        .badge-holding-regs { background: rgba(16, 185, 129, 0.12); color: var(--accent-emerald); border: 1px solid rgba(16, 185, 129, 0.2); }

        .badge-type {
            background: rgba(255, 255, 255, 0.05);
            color: var(--text-secondary);
            border: 1px solid var(--border-color);
        }

        /* Protocol Grid view */
        .proto-grid {
            display: flex;
            flex-direction: column;
            gap: 0.25rem;
            font-size: 0.75rem;
            font-family: monospace;
        }

        .proto-badge {
            display: flex;
            justify-content: space-between;
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid rgba(255, 255, 255, 0.04);
            padding: 0.2rem 0.5rem;
            border-radius: 4px;
        }

        .proto-name { color: var(--text-secondary); font-weight: 600; font-family: var(--font-main); font-size: 0.65rem; }
        .proto-addr { color: var(--accent-cyan); font-weight: bold; }

        /* Force Write controls styling */
        .force-write-container {
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        .write-input {
            width: 80px;
            padding: 0.4rem 0.6rem;
            font-size: 0.85rem;
            text-align: center;
        }

        .btn-write {
            background: rgba(6, 182, 212, 0.1);
            color: var(--accent-cyan);
            border: 1px solid rgba(6, 182, 212, 0.3);
            border-radius: 6px;
            padding: 0.4rem 0.75rem;
            font-size: 0.8rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s ease;
        }

        .btn-write:hover {
            background: var(--accent-cyan);
            color: white;
        }

        /* iOS Switch Toggle style */
        .switch {
            position: relative;
            display: inline-block;
            width: 44px;
            height: 24px;
        }

        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: rgba(255, 255, 255, 0.08);
            transition: .3s;
            border-radius: 24px;
            border: 1px solid var(--border-color);
        }

        .slider:before {
            position: absolute;
            content: "";
            height: 16px;
            width: 16px;
            left: 3px;
            bottom: 3px;
            background-color: var(--text-secondary);
            transition: .3s;
            border-radius: 50%;
        }

        input:checked + .slider {
            background-color: rgba(16, 185, 129, 0.2);
            border-color: var(--accent-emerald);
        }

        input:checked + .slider:before {
            transform: translateX(20px);
            background-color: var(--accent-emerald);
            box-shadow: 0 0 8px var(--accent-emerald);
        }

        .btn-delete {
            background: transparent;
            color: var(--accent-rose);
            border: 1px solid rgba(244, 63, 94, 0.2);
            border-radius: 6px;
            padding: 0.4rem;
            cursor: pointer;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            transition: all 0.2s ease;
        }

        .btn-delete:hover {
            background: var(--accent-rose);
            color: white;
            box-shadow: 0 0 10px rgba(244, 63, 94, 0.4);
        }

        /* Notification Toast toast */
        #toast-container {
            position: fixed;
            bottom: 2rem;
            right: 2rem;
            z-index: 1000;
            display: flex;
            flex-direction: column;
            gap: 0.5rem;
        }

        .toast {
            background: rgba(17, 24, 39, 0.9);
            border-left: 4px solid var(--accent-cyan);
            border-top: 1px solid var(--border-color);
            border-bottom: 1px solid var(--border-color);
            border-right: 1px solid var(--border-color);
            padding: 1rem 1.5rem;
            border-radius: 8px;
            color: var(--text-primary);
            font-size: 0.9rem;
            font-weight: 500;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.5);
            display: flex;
            align-items: center;
            gap: 0.75rem;
            animation: slideIn 0.3s ease-out forwards;
            backdrop-filter: blur(8px);
        }

        @keyframes slideIn {
            from { transform: translateX(100%); opacity: 0; }
            to { transform: translateX(0); opacity: 1; }
        }

        .toast-success { border-left-color: var(--accent-emerald); }
        .toast-error { border-left-color: var(--accent-rose); }
    </style>
</head>
<body>

    <header>
        <div class="logo-section">
            <h1>vPLC Core Configurator</h1>
            <p>Embedded Virtual PLC Runtime Environment</p>
        </div>
        <div class="system-status">
            <div class="status-badge">
                <span class="status-dot"></span>
                <span>SYSTEM ONLINE</span>
            </div>
            <div class="status-badge" style="color: var(--text-secondary);">
                <span>Scan Delay: </span>
                <span id="scan-delay" style="color: var(--accent-cyan); font-weight: 600; margin-left: 0.25rem;">20.0ms</span>
            </div>
            <div class="status-badge" style="color: var(--text-secondary);">
                <span>Mode: </span>
                <span id="plc-mode" style="color: var(--accent-violet); font-weight: 600; margin-left: 0.25rem;">AUTO</span>
            </div>
        </div>
    </header>

    <div class="main-container">
        <!-- Create Tag Form Card -->
        <div class="card">
            <div class="card-title">신규 메모리 맵핑 추가</div>
            <form id="tag-form" onsubmit="onCreateTag(event)">
                <div class="form-group">
                    <label for="tagName">태그(변수) 이름</label>
                    <input type="text" id="tagName" class="form-control" placeholder="예: Motor_Start" required pattern="[A-Za-z0-9_]+">
                </div>

                <div class="form-group">
                    <label for="tagArea">가상 메모리 영역 (Modbus)</label>
                    <select id="tagArea" class="form-control" required onchange="onAreaChange()">
                        <option value="Coils">Coils (0x, R/W Bool)</option>
                        <option value="DiscreteInputs">Discrete Inputs (1x, R-Only Bool)</option>
                        <option value="InputRegisters">Input Registers (3x, R-Only Word)</option>
                        <option value="HoldingRegisters" selected>Holding Registers (4x, R/W Word)</option>
                    </select>
                </div>

                <div class="form-group">
                    <label for="tagAddress">어드레스 주소 번지 (0-based)</label>
                    <input type="number" id="tagAddress" class="form-control" value="0" min="0" required oninput="updateAddressPreview()">
                </div>

                <div class="form-group">
                    <label for="tagType">데이터 형식</label>
                    <select id="tagType" class="form-control" required>
                        <option value="UINT16" selected>UINT16 (16비트 정수)</option>
                        <option value="INT16">INT16 (부호 있는 16비트 정수)</option>
                        <option value="BOOL" disabled>BOOL (비트 데이터)</option>
                    </select>
                </div>

                <div class="form-group">
                    <label>실시간 프로토콜별 주소 변환</label>
                    <div class="preview-box">
                        <div class="preview-item">
                            <span class="preview-label">Siemens S7</span>
                            <span class="preview-value" id="preview-s7">-</span>
                        </div>
                        <div class="preview-item">
                            <span class="preview-label">Mitsubishi MC</span>
                            <span class="preview-value" id="preview-mc">-</span>
                        </div>
                        <div class="preview-item">
                            <span class="preview-label">LS XGT (IEC)</span>
                            <span class="preview-value" id="preview-xgt">-</span>
                        </div>
                        <div class="preview-item">
                            <span class="preview-label">Modbus TCP</span>
                            <span class="preview-value" id="preview-modbus">-</span>
                        </div>
                    </div>
                </div>

                <div class="form-group">
                    <label for="tagDesc">설명</label>
                    <input type="text" id="tagDesc" class="form-control" placeholder="예: 메인 모터 가동 기동">
                </div>

                <button type="submit" class="btn" style="margin-top: 0.5rem;">가상 주소 매핑 등록</button>
            </form>
        </div>

        <!-- Live Tag List Card -->
        <div class="card" style="overflow: hidden;">
            <div class="card-title">
                실시간 동적 메모리 모니터링
                <span style="font-size: 0.8rem; font-weight: normal; color: var(--text-secondary);">스레드 세이프 실시간 맵핑 뷰어</span>
            </div>
            <div class="table-wrapper">
                <table>
                    <thead>
                        <tr>
                            <th>변수명</th>
                            <th>설명</th>
                            <th>메모리 영역</th>
                            <th>타입</th>
                            <th>프로토콜 매핑 주소</th>
                            <th>현재값</th>
                            <th style="width: 150px;">강제 제어 (Force Write)</th>
                            <th style="width: 50px;">삭제</th>
                        </tr>
                    </thead>
                    <tbody id="tag-table-body">
                        <tr>
                            <td colspan="8" style="text-align: center; color: var(--text-secondary); padding: 3rem;">
                                등록된 동적 주소 매핑이 없습니다. 왼쪽 양식을 통해 메모리를 할당해 주세요.
                            </td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </div>
    </div>

    <!-- Dynamic Mapping Editor Card -->
    <div class="card" style="margin-top: 2rem;">
        <div class="card-title">
            실시간 C++ 메모리 교차 매핑 엔진 (mappings.json)
            <div style="display: flex; gap: 0.5rem;">
                <button class="btn-write" onclick="loadMappingPreset('mirror')">기본 양방향 미러링</button>
                <button class="btn-write" onclick="loadMappingPreset('s7_mc')">S7 ➡️ MC</button>
                <button class="btn-write" onclick="loadMappingPreset('mc_ls')">MC ➡️ LS</button>
            </div>
        </div>
        <div style="display: flex; flex-direction: column; gap: 1rem;">
            <p style="font-size: 0.85rem; color: var(--text-secondary); line-height: 1.4;">
                Modbus, Siemens S7, Mitsubishi MC, LS Electric XGT의 모든 고유 메모리 영역 간에 <strong>양방향 실시간 미러링 및 교차 매핑</strong>을 수행합니다. 
                좌측 Presets 버튼을 누르거나 아래 JSON을 직접 수정한 후 <strong>"매핑 규칙 실시간 핫로드(Hot-Reload)"</strong>를 누르면 vPLC 코어에 즉시 반영됩니다.
            </p>
            <textarea id="mapping-json" class="form-control" style="font-family: monospace; height: 350px; font-size: 0.85rem; line-height: 1.5; background: rgba(0,0,0,0.3); color: #06b6d4; border-color: rgba(6, 182, 212, 0.35); resize: vertical;"></textarea>
            <button class="btn" onclick="saveMappings()">매핑 규칙 실시간 핫로드 (Hot-Reload)</button>
        </div>
    </div>

    <div id="toast-container"></div>

    <script>
        // Track rendered tag names to see if the table structure needs rebuilding
        let currentTagListHash = "";

        // Calculate mapped protocol address
        function calculateProtocolAddresses(area, address) {
            let modbus = "-";
            let s7 = "-";
            let mc = "-";
            let xgt = "-";

            const addr = parseInt(address) || 0;

            if (area === "Coils") {
                modbus = `0000${addr + 1}`.slice(-5);
                const byte = Math.floor(addr / 8);
                const bit = addr % 8;
                s7 = `Q${byte}.${bit}`;
                mc = `Y${addr}`;
                xgt = `%QX${byte}.${bit}`;
            } else if (area === "DiscreteInputs") {
                modbus = `1000${addr + 1}`.slice(-5);
                const byte = Math.floor(addr / 8);
                const bit = addr % 8;
                s7 = `I${byte}.${bit}`;
                mc = `X${addr}`;
                xgt = `%IX${byte}.${bit}`;
            } else if (area === "InputRegisters") {
                modbus = `3000${addr + 1}`.slice(-5);
                s7 = `-`;
                mc = `-`;
                xgt = `%IW${addr}`;
            } else if (area === "HoldingRegisters") {
                modbus = `4000${addr + 1}`.slice(-5);
                s7 = `DB1.DBW${addr * 2}`;
                mc = `D${addr}`;
                xgt = `%MW${addr}`;
            }

            return { modbus, s7, mc, xgt };
        }

        // Live Address Calculation on input change
        function updateAddressPreview() {
            const area = document.getElementById("tagArea").value;
            const address = document.getElementById("tagAddress").value;
            
            const mapping = calculateProtocolAddresses(area, address);
            
            document.getElementById("preview-s7").textContent = mapping.s7;
            document.getElementById("preview-mc").textContent = mapping.mc;
            document.getElementById("preview-xgt").textContent = mapping.xgt;
            document.getElementById("preview-modbus").textContent = mapping.modbus;
        }

        // Set allowed Types based on Selected Area
        function onAreaChange() {
            const area = document.getElementById("tagArea").value;
            const typeSelect = document.getElementById("tagType");
            const boolOption = typeSelect.querySelector("option[value='BOOL']");
            const uintOption = typeSelect.querySelector("option[value='UINT16']");
            const intOption = typeSelect.querySelector("option[value='INT16']");

            if (area === "Coils" || area === "DiscreteInputs") {
                boolOption.disabled = false;
                boolOption.selected = true;
                uintOption.disabled = true;
                intOption.disabled = true;
            } else {
                boolOption.disabled = true;
                uintOption.disabled = false;
                intOption.disabled = false;
                if (typeSelect.value === "BOOL") {
                    uintOption.selected = true;
                }
            }
            updateAddressPreview();
        }

        // Initialize state
        onAreaChange();

        // Display Alert Toasts
        function showToast(message, type = 'info') {
            const container = document.getElementById("toast-container");
            const toast = document.createElement("div");
            toast.className = `toast toast-${type}`;
            
            let icon = 'ℹ️';
            if (type === 'success') icon = '🟢';
            if (type === 'error') icon = '🔴';
            
            toast.innerHTML = `<span>${icon}</span> <span>${message}</span>`;
            container.appendChild(toast);
            
            setTimeout(() => {
                toast.style.animation = 'slideIn 0.3s reverse ease-in forwards';
                setTimeout(() => toast.remove(), 300);
            }, 3000);
        }

        // Fetch System status info
        async function fetchSystemStatus() {
            try {
                const response = await fetch('/api/system');
                if (response.ok) {
                    const data = await response.json();
                    document.getElementById("scan-delay").textContent = `${data.cycleTimeMs.toFixed(1)}ms`;
                    document.getElementById("plc-mode").textContent = data.mode;
                    const modeBadge = document.getElementById("plc-mode");
                    if (data.mode === 'AUTO') {
                        modeBadge.style.color = 'var(--accent-cyan)';
                    } else {
                        modeBadge.style.color = 'var(--accent-rose)';
                    }
                }
            } catch (err) {
                console.error("Failed to fetch system status:", err);
            }
        }

        // Fetch Tag lists and render (with selective updating)
        async function fetchTags() {
            try {
                const response = await fetch('/api/tags');
                if (!response.ok) throw new Error("API Error");
                
                const tags = await response.json();
                
                // Construct a unique hash of the tag names to see if list has changed
                const hash = tags.map(t => `${t.name}-${t.area}-${t.address}-${t.type}`).join('|');
                
                if (hash !== currentTagListHash) {
                    currentTagListHash = hash;
                    rebuildTagTableStructure(tags);
                }
                
                // Update live values inside the table targeting specific DOM elements
                updateLiveValues(tags);
            } catch (err) {
                console.error("Failed to fetch tags:", err);
            }
        }

        function getAreaBadge(area) {
            if (area === "Coils") return `<span class="badge badge-coils">Coils (0x)</span>`;
            if (area === "DiscreteInputs") return `<span class="badge badge-inputs">Discrete Inputs (1x)</span>`;
            if (area === "InputRegisters") return `<span class="badge badge-input-regs">Input Regs (3x)</span>`;
            if (area === "HoldingRegisters") return `<span class="badge badge-holding-regs">Holding Regs (4x)</span>`;
            return area;
        }

        // Completely rebuild table rows only when tag schema changes (Prevents losing focus!)
        function rebuildTagTableStructure(tags) {
            const tbody = document.getElementById("tag-table-body");
            if (tags.length === 0) {
                tbody.innerHTML = `
                    <tr>
                        <td colspan="8" style="text-align: center; color: var(--text-secondary); padding: 3rem;">
                            등록된 동적 주소 매핑이 없습니다. 왼쪽 양식을 통해 메모리를 할당해 주세요.
                        </td>
                    </tr>`;
                return;
            }

            tbody.innerHTML = tags.map(tag => {
                const isBool = tag.type === "BOOL";
                const isChecked = tag.value === "1" ? "checked" : "";
                
                let forceControlHtml = "";
                if (isBool) {
                    forceControlHtml = `
                        <label class="switch">
                            <input type="checkbox" id="ctrl-${tag.name}" ${isChecked} onchange="onForceWrite('${tag.name}', this.checked ? '1' : '0')">
                            <span class="slider"></span>
                        </label>
                    `;
                } else {
                    forceControlHtml = `
                        <div class="force-write-container">
                            <input type="number" class="form-control write-input" id="input-${tag.name}" value="${tag.value}">
                            <button class="btn-write" onclick="onForceWordWrite('${tag.name}')">쓰기</button>
                        </div>
                    `;
                }

                // Render Protocol Mappings preview for table
                const mapping = calculateProtocolAddresses(tag.area, tag.address);
                const mappingHtml = `
                    <div class="proto-grid">
                        <div class="proto-badge"><span class="proto-name">Modbus</span><span class="proto-addr">${mapping.modbus}</span></div>
                        <div class="proto-badge"><span class="proto-name">Siemens S7</span><span class="proto-addr">${mapping.s7}</span></div>
                        <div class="proto-badge"><span class="proto-name">Mitsubishi MC</span><span class="proto-addr">${mapping.mc}</span></div>
                        <div class="proto-badge"><span class="proto-name">LS XGT</span><span class="proto-addr">${mapping.xgt}</span></div>
                    </div>
                `;

                return `
                    <tr id="row-${tag.name}">
                        <td style="font-weight: 600; color: var(--accent-cyan); font-family: var(--font-title);">${tag.name}</td>
                        <td style="color: var(--text-secondary); font-size: 0.85rem;">${tag.description || '-'}</td>
                        <td>${getAreaBadge(tag.area)}</td>
                        <td><span class="badge badge-type">${tag.type}</span></td>
                        <td>${mappingHtml}</td>
                        <td><span id="val-display-${tag.name}">-</span></td>
                        <td>${forceControlHtml}</td>
                        <td>
                            <button class="btn-delete" onclick="onDeleteTag('${tag.name}')">
                                <svg style="width: 16px; height: 16px;" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
                                </svg>
                            </button>
                        </td>
                    </tr>
                `;
            }).join('');
        }

        // Targeted updates to avoid replacing DOM and preserving user focus
        function updateLiveValues(tags) {
            tags.forEach(tag => {
                const isBool = tag.type === "BOOL";
                
                // 1. Update text display element
                const displayEl = document.getElementById(`val-display-${tag.name}`);
                if (displayEl) {
                    let valDisplay = tag.value;
                    if (isBool) {
                        valDisplay = tag.value === "1" ? "<span style='color: var(--accent-emerald); font-weight:600;'>ON (1)</span>" : "<span style='color: var(--text-secondary);'>OFF (0)</span>";
                    } else {
                        valDisplay = `<span style='color: ${tag.value !== "0" ? 'var(--accent-cyan)' : 'var(--text-secondary)'}; font-weight:600;'>${tag.value}</span>`;
                    }
                    displayEl.innerHTML = valDisplay;
                }

                // 2. Update controls values (Skip if element is focused!)
                if (isBool) {
                    const switchEl = document.getElementById(`ctrl-${tag.name}`);
                    if (switchEl && document.activeElement !== switchEl) {
                        switchEl.checked = tag.value === "1";
                    }
                } else {
                    const inputEl = document.getElementById(`input-${tag.name}`);
                    if (inputEl) {
                        // Skip updating input value if user is actively focused on it to type!
                        if (document.activeElement !== inputEl) {
                            inputEl.value = tag.value;
                        }
                    }
                }
            });
        }

        // Post new Tag
        async function onCreateTag(e) {
            e.preventDefault();
            const tag = {
                name: document.getElementById("tagName").value,
                area: document.getElementById("tagArea").value,
                address: parseInt(document.getElementById("tagAddress").value),
                type: document.getElementById("tagType").value,
                description: document.getElementById("tagDesc").value
            };

            try {
                const response = await fetch('/api/tags', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(tag)
                });
                
                if (response.ok) {
                    showToast("태그 매핑이 정상적으로 등록되었습니다.", "success");
                    document.getElementById("tag-form").reset();
                    document.getElementById("tagAddress").value = 0;
                    onAreaChange();
                    fetchTags();
                } else {
                    const errMsg = await response.text();
                    showToast("등록 실패: " + errMsg, "error");
                }
            } catch (err) {
                showToast("네트워크 오류가 발생했습니다.", "error");
            }
        }

        // Delete Tag
        async function onDeleteTag(name) {
            if (!confirm(`태그 [${name}]의 메모리 할당 매핑을 삭제하시겠습니까?`)) return;

            try {
                const response = await fetch(`/api/tags?name=${encodeURIComponent(name)}`, {
                    method: 'DELETE'
                });
                if (response.ok) {
                    showToast("태그 매핑이 삭제되었습니다.", "success");
                    fetchTags();
                } else {
                    showToast("삭제를 실패하였습니다.", "error");
                }
            } catch (err) {
                showToast("네트워크 오류가 발생했습니다.", "error");
            }
        }

        // Force Write value
        async function onForceWrite(name, value) {
            try {
                const response = await fetch('/api/tags/write', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name, value })
                });
                if (response.ok) {
                    showToast(`태그 [${name}] 강제 제어 성공`, "success");
                    // Refresh values immediately
                    const fetchResp = await fetch('/api/tags');
                    if (fetchResp.ok) {
                        const tags = await fetchResp.json();
                        updateLiveValues(tags);
                    }
                } else {
                    showToast("제어 실패", "error");
                }
            } catch (err) {
                showToast("네트워크 오류", "error");
            }
        }

        // Force Word value write
        function onForceWordWrite(name) {
            const inputVal = document.getElementById(`input-${name}`).value;
            onForceWrite(name, inputVal);
        }

        // Presets data
        const presets = {
            mirror: [
                { "src": "MODBUS.IR.0", "dst": "S7.DB1.W.64" },
                { "src": "MODBUS.DI.0", "dst": "S7.PE.0" },
                { "src": "MODBUS.DI.1", "dst": "S7.PE.1" },
                { "src": "MODBUS.DI.2", "dst": "S7.PE.2" },
                { "src": "MODBUS.DI.3", "dst": "S7.PE.3" },
                { "src": "MODBUS.DI.4", "dst": "S7.PE.4" },
                { "src": "MODBUS.DI.5", "dst": "S7.PE.5" },
                { "src": "MODBUS.DI.6", "dst": "S7.PE.6" },
                { "src": "MODBUS.DI.7", "dst": "S7.PE.7" },
                { "src": "MODBUS.DI.0", "dst": "MC.X.0" },
                { "src": "MODBUS.DI.1", "dst": "MC.X.1" },
                { "src": "MODBUS.DI.2", "dst": "MC.X.2" },
                { "src": "MODBUS.DI.3", "dst": "MC.X.3" },
                { "src": "MODBUS.DI.4", "dst": "MC.X.4" },
                { "src": "MODBUS.DI.5", "dst": "MC.X.5" },
                { "src": "MODBUS.DI.6", "dst": "MC.X.6" },
                { "src": "MODBUS.DI.7", "dst": "MC.X.7" },
                { "src": "MODBUS.DI.0", "dst": "LS.I.0" },
                { "src": "MODBUS.DI.1", "dst": "LS.I.1" },
                { "src": "MODBUS.DI.2", "dst": "LS.I.2" },
                { "src": "MODBUS.DI.3", "dst": "LS.I.3" },
                { "src": "MODBUS.DI.4", "dst": "LS.I.4" },
                { "src": "MODBUS.DI.5", "dst": "LS.I.5" },
                { "src": "MODBUS.DI.6", "dst": "LS.I.6" },
                { "src": "MODBUS.DI.7", "dst": "LS.I.7" },
                { "src": "MODBUS.COIL.0", "dst": "S7.PA.0" },
                { "src": "MODBUS.COIL.1", "dst": "S7.PA.1" },
                { "src": "MODBUS.COIL.2", "dst": "S7.PA.2" },
                { "src": "MODBUS.COIL.3", "dst": "S7.PA.3" },
                { "src": "MODBUS.COIL.4", "dst": "S7.PA.4" },
                { "src": "MODBUS.COIL.5", "dst": "S7.PA.5" },
                { "src": "MODBUS.COIL.6", "dst": "S7.PA.6" },
                { "src": "MODBUS.COIL.7", "dst": "S7.PA.7" },
                { "src": "MODBUS.COIL.0", "dst": "MC.Y.0" },
                { "src": "MODBUS.COIL.1", "dst": "MC.Y.1" },
                { "src": "MODBUS.COIL.2", "dst": "MC.Y.2" },
                { "src": "MODBUS.COIL.3", "dst": "MC.Y.3" },
                { "src": "MODBUS.COIL.4", "dst": "MC.Y.4" },
                { "src": "MODBUS.COIL.5", "dst": "MC.Y.5" },
                { "src": "MODBUS.COIL.6", "dst": "MC.Y.6" },
                { "src": "MODBUS.COIL.7", "dst": "MC.Y.7" },
                { "src": "MODBUS.COIL.0", "dst": "LS.Q.0" },
                { "src": "MODBUS.COIL.1", "dst": "LS.Q.1" },
                { "src": "MODBUS.COIL.2", "dst": "LS.Q.2" },
                { "src": "MODBUS.COIL.3", "dst": "LS.Q.3" },
                { "src": "MODBUS.COIL.4", "dst": "LS.Q.4" },
                { "src": "MODBUS.COIL.5", "dst": "LS.Q.5" },
                { "src": "MODBUS.COIL.6", "dst": "LS.Q.6" },
                { "src": "MODBUS.COIL.7", "dst": "LS.Q.7" },
                { "src": "MODBUS.COIL.0", "dst": "LS.M.0" },
                { "src": "MODBUS.COIL.1", "dst": "LS.M.1" },
                { "src": "MODBUS.COIL.2", "dst": "LS.M.2" },
                { "src": "MODBUS.COIL.3", "dst": "LS.M.3" },
                { "src": "MODBUS.COIL.4", "dst": "LS.M.4" },
                { "src": "MODBUS.COIL.5", "dst": "LS.M.5" },
                { "src": "MODBUS.COIL.6", "dst": "LS.M.6" },
                { "src": "MODBUS.COIL.7", "dst": "LS.M.7" },
                { "src": "MODBUS.HR.0", "dst": "S7.DB1.W.0" },
                { "src": "MODBUS.HR.1", "dst": "S7.DB1.W.1" },
                { "src": "MODBUS.HR.2", "dst": "S7.DB1.W.2" },
                { "src": "MODBUS.HR.3", "dst": "S7.DB1.W.3" },
                { "src": "MODBUS.HR.4", "dst": "S7.DB1.W.4" },
                { "src": "MODBUS.HR.5", "dst": "S7.DB1.W.5" },
                { "src": "MODBUS.HR.6", "dst": "S7.DB1.W.6" },
                { "src": "MODBUS.HR.7", "dst": "S7.DB1.W.7" },
                { "src": "MODBUS.HR.0", "dst": "MC.D.0" },
                { "src": "MODBUS.HR.1", "dst": "MC.D.1" },
                { "src": "MODBUS.HR.2", "dst": "MC.D.2" },
                { "src": "MODBUS.HR.3", "dst": "MC.D.3" },
                { "src": "MODBUS.HR.4", "dst": "MC.D.4" },
                { "src": "MODBUS.HR.5", "dst": "MC.D.5" },
                { "src": "MODBUS.HR.6", "dst": "MC.D.6" },
                { "src": "MODBUS.HR.7", "dst": "MC.D.7" },
                { "src": "MODBUS.HR.0", "dst": "LS.W.0" },
                { "src": "MODBUS.HR.1", "dst": "LS.W.1" },
                { "src": "MODBUS.HR.2", "dst": "LS.W.2" },
                { "src": "MODBUS.HR.3", "dst": "LS.W.3" },
                { "src": "MODBUS.HR.4", "dst": "LS.W.4" },
                { "src": "MODBUS.HR.5", "dst": "LS.W.5" },
                { "src": "MODBUS.HR.6", "dst": "LS.W.6" },
                { "src": "MODBUS.HR.7", "dst": "LS.W.7" }
            ],
            s7_mc: [
                { "src": "S7.PE.0", "dst": "MC.Y.0" },
                { "src": "S7.PE.1", "dst": "MC.Y.1" },
                { "src": "S7.PE.2", "dst": "MC.Y.2" },
                { "src": "S7.PE.3", "dst": "MC.Y.3" },
                { "src": "S7.PA.0", "dst": "MC.X.0" },
                { "src": "S7.PA.1", "dst": "MC.X.1" },
                { "src": "S7.PA.2", "dst": "MC.X.2" },
                { "src": "S7.PA.3", "dst": "MC.X.3" },
                { "src": "S7.DB1.W.0", "dst": "MC.D.0" },
                { "src": "S7.DB1.W.1", "dst": "MC.D.1" },
                { "src": "S7.DB1.W.2", "dst": "MC.D.2" },
                { "src": "S7.DB1.W.3", "dst": "MC.D.3" }
            ],
            mc_ls: [
                { "src": "MC.X.0", "dst": "LS.I.0" },
                { "src": "MC.X.1", "dst": "LS.I.1" },
                { "src": "MC.X.2", "dst": "LS.I.2" },
                { "src": "MC.X.3", "dst": "LS.I.3" },
                { "src": "MC.Y.0", "dst": "LS.Q.0" },
                { "src": "MC.Y.1", "dst": "LS.Q.1" },
                { "src": "MC.Y.2", "dst": "LS.Q.2" },
                { "src": "MC.Y.3", "dst": "LS.Q.3" },
                { "src": "MC.D.0", "dst": "LS.W.0" },
                { "src": "MC.D.1", "dst": "LS.W.1" },
                { "src": "MC.D.2", "dst": "LS.W.2" },
                { "src": "MC.D.3", "dst": "LS.W.3" }
            ]
        };

        function loadMappingPreset(presetName) {
            if (presets[presetName]) {
                document.getElementById("mapping-json").value = JSON.stringify(presets[presetName], null, 4);
                showToast(`${presetName === 'mirror' ? '기본 양방향 미러링' : presetName === 's7_mc' ? 'S7 ➡️ MC' : 'MC ➡️ LS'} 프리셋을 불러왔습니다. 하단 버튼을 클릭해 핫로드 하세요.`, "info");
            }
        }

        async function fetchMappings() {
            try {
                const response = await fetch('/api/mappings');
                if (response.ok) {
                    const mappings = await response.json();
                    document.getElementById("mapping-json").value = JSON.stringify(mappings, null, 4);
                }
            } catch (err) {
                console.error("Failed to fetch mappings:", err);
            }
        }

        async function saveMappings() {
            const rawJson = document.getElementById("mapping-json").value;
            let mappingsObj;
            try {
                mappingsObj = JSON.parse(rawJson);
            } catch (err) {
                showToast("JSON 형식이 올바르지 않습니다. 구문을 확인해 주세요.", "error");
                return;
            }

            try {
                const response = await fetch('/api/mappings', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(mappingsObj)
                });
                
                if (response.ok) {
                    showToast("매핑 설정이 vPLC 코어에 실시간 핫로드(Hot-Reload) 되었습니다!", "success");
                } else {
                    const errMsg = await response.text();
                    showToast("핫로드 실패: " + errMsg, "error");
                }
            } catch (err) {
                showToast("네트워크 오류가 발생했습니다.", "error");
            }
        }

        // Start polling loop
        fetchTags();
        fetchSystemStatus();
        fetchMappings();
        setInterval(fetchTags, 500);
        setInterval(fetchSystemStatus, 1000);
    </script>
</body>
</html>
)HTML";

} // namespace WebUI

#endif // WEB_UI_HPP
