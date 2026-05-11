#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct CompileResult {
    bool requestOk = false;
    bool compileOk = false;
    int exitCode = -1;
    std::string message;
    std::string compilerOutput;
    fs::path inputPath;
    fs::path outputDir;
    std::map<std::string, std::string> outputs;
};

static std::string toLowerAscii(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

static std::wstring toLowerWide(std::wstring s) {
    for (wchar_t& ch : s) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return s;
}

static bool pathStartsWith(const fs::path& p, const fs::path& base) {
    fs::path pn = p.lexically_normal();
    fs::path bn = base.lexically_normal();
    auto pit = pn.begin();
    auto bit = bn.begin();
    for (; bit != bn.end(); ++bit, ++pit) {
        if (pit == pn.end()) return false;
        if (toLowerWide(pit->wstring()) != toLowerWide(bit->wstring())) {
            return false;
        }
    }
    return true;
}

static std::string jsonEscape(const std::string& in) {
    std::ostringstream oss;
    for (unsigned char ch : in) {
        switch (ch) {
            case '\"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (ch < 0x20) {
                    oss << "\\u00";
                    const char* hex = "0123456789ABCDEF";
                    oss << hex[(ch >> 4) & 0xF] << hex[ch & 0xF];
                } else {
                    oss << static_cast<char>(ch);
                }
                break;
        }
    }
    return oss.str();
}

static bool readFileText(const fs::path& path, std::string& text, size_t maxBytes = 512 * 1024) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    text = ss.str();
    if (text.size() > maxBytes) {
        text.resize(maxBytes);
        text += "\n...[truncated]";
    }
    return true;
}

static fs::path findProjectRoot(const fs::path& start) {
    fs::path cur = start;
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(cur / "CMakeLists.txt") && fs::exists(cur / "src")) {
            return cur;
        }
        if (!cur.has_parent_path()) break;
        fs::path parent = cur.parent_path();
        if (parent == cur) break;
        cur = parent;
    }
    return {};
}

static std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

static std::string parseJsonStringField(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return "";
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos >= json.size() || json[pos] != '"') return "";
    ++pos;
    std::string out;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '"') {
            return out;
        }
        if (c == '\\' && pos < json.size()) {
            char esc = json[pos++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(esc); break;
            }
        } else {
            out.push_back(c);
        }
    }
    return "";
}

static std::string urlDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    auto hexVal = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    };
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '+' ) {
            out.push_back(' ');
            continue;
        }
        if (c == '%' && i + 2 < in.size()) {
            int hi = hexVal(in[i + 1]);
            int lo = hexVal(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}

static std::string getQueryParam(const std::string& query, const std::string& key) {
    size_t start = 0;
    while (start <= query.size()) {
        size_t end = query.find('&', start);
        if (end == std::string::npos) end = query.size();
        std::string pair = query.substr(start, end - start);
        size_t eq = pair.find('=');
        std::string k = (eq == std::string::npos) ? pair : pair.substr(0, eq);
        std::string v = (eq == std::string::npos) ? "" : pair.substr(eq + 1);
        if (k == key) {
            return urlDecode(v);
        }
        if (end == query.size()) break;
        start = end + 1;
    }
    return "";
}

static std::string httpReason(int statusCode) {
    switch (statusCode) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

static void sendAll(SOCKET sock, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(sock, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
}

static void sendResponse(SOCKET sock, int statusCode, const std::string& contentType, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " " << httpReason(statusCode) << "\r\n";
    oss << "Content-Type: " << contentType << "; charset=UTF-8\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    sendAll(sock, oss.str());
}

static bool parseRequest(SOCKET sock, HttpRequest& req) {
    std::string raw;
    raw.reserve(8192);
    std::array<char, 4096> buf{};
    size_t headerEnd = std::string::npos;
    while (true) {
        int n = recv(sock, buf.data(), static_cast<int>(buf.size()), 0);
        if (n <= 0) return false;
        raw.append(buf.data(), static_cast<size_t>(n));
        headerEnd = raw.find("\r\n\r\n");
        if (headerEnd != std::string::npos) break;
        if (raw.size() > 1024 * 1024) return false;
    }

    std::string headerText = raw.substr(0, headerEnd);
    std::istringstream hs(headerText);
    std::string firstLine;
    if (!std::getline(hs, firstLine)) return false;
    if (!firstLine.empty() && firstLine.back() == '\r') firstLine.pop_back();

    {
        std::istringstream ls(firstLine);
        std::string version;
        ls >> req.method >> req.path >> version;
        if (req.method.empty() || req.path.empty()) return false;
    }

    std::string line;
    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = toLowerAscii(trim(line.substr(0, colon)));
        std::string val = trim(line.substr(colon + 1));
        req.headers[key] = val;
    }

    size_t bodyStart = headerEnd + 4;
    size_t contentLen = 0;
    auto it = req.headers.find("content-length");
    if (it != req.headers.end()) {
        try {
            contentLen = static_cast<size_t>(std::stoul(it->second));
        } catch (...) {
            contentLen = 0;
        }
    }

    if (raw.size() < bodyStart + contentLen) {
        while (raw.size() < bodyStart + contentLen) {
            int n = recv(sock, buf.data(), static_cast<int>(buf.size()), 0);
            if (n <= 0) return false;
            raw.append(buf.data(), static_cast<size_t>(n));
            if (raw.size() > 4 * 1024 * 1024) return false;
        }
    }
    if (contentLen > 0 && bodyStart + contentLen <= raw.size()) {
        req.body = raw.substr(bodyStart, contentLen);
    }
    return true;
}

static std::string buildIndexHtml() {
    return R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>SNL Compiler Web UI</title>
  <style>
    :root {
      --bg: #f4f6f8;
      --card: #ffffff;
      --ink: #1f2937;
      --muted: #6b7280;
      --brand: #0f766e;
      --border: #d1d5db;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
      background: linear-gradient(160deg, #f9fafb 0%, #e8f5f2 100%);
      color: var(--ink);
    }
    .wrap { max-width: 1100px; margin: 24px auto; padding: 0 16px; }
    .card {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 14px;
      padding: 16px;
      box-shadow: 0 6px 20px rgba(15, 118, 110, 0.08);
      margin-bottom: 14px;
    }
    h1 { margin: 0 0 8px; font-size: 22px; }
    h2 { margin: 0 0 10px; font-size: 16px; }
    p.tip { margin: 0; color: var(--muted); }
    .row { display: grid; grid-template-columns: 1fr 130px 130px 120px; gap: 10px; margin-top: 14px; }
    select, button {
      height: 40px;
      border-radius: 10px;
      border: 1px solid var(--border);
      padding: 0 10px;
      font-size: 14px;
      background: #fff;
      color: var(--ink);
    }
    button {
      cursor: pointer;
      background: var(--brand);
      color: #fff;
      border-color: var(--brand);
      font-weight: 600;
    }
    button.secondary {
      background: #fff;
      color: var(--ink);
      border-color: var(--border);
    }
    .status {
      margin-top: 10px;
      padding: 10px;
      border-radius: 10px;
      background: #f9fafb;
      border: 1px solid var(--border);
      white-space: pre-wrap;
      font-size: 13px;
    }
    .tabs { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 10px; }
    .tab {
      border: 1px solid var(--border);
      background: #fff;
      color: var(--ink);
      border-radius: 999px;
      padding: 6px 12px;
      cursor: pointer;
      font-size: 13px;
    }
    .tab.active {
      background: var(--brand);
      color: #fff;
      border-color: var(--brand);
    }
    pre {
      margin: 0;
      max-height: 480px;
      overflow: auto;
      padding: 12px;
      border-radius: 10px;
      border: 1px solid var(--border);
      background: #111827;
      color: #e5e7eb;
      font-size: 12px;
      line-height: 1.45;
      white-space: pre;
    }
    @media (max-width: 860px) {
      .row { grid-template-columns: 1fr 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>SNL Compiler Web UI</h1>
      <p class="tip">Select a .snl file under tests/, compile it, and inspect outputs.</p>
      <div class="row">
        <select id="fileSelect"></select>
        <select id="modeSelect">
          <option value="all">all</option>
        </select>
        <button class="secondary" id="refreshBtn">Refresh</button>
        <button id="compileBtn">Compile</button>
      </div>
      <div id="status" class="status">Ready.</div>
    </div>

    <div class="card">
      <h2>Selected Source (.snl)</h2>
      <pre id="sourceView">No file selected.</pre>
    </div>

    <div class="card">
      <div id="tabs" class="tabs"></div>
      <pre id="contentView">No results yet.</pre>
    </div>
  </div>

  <script>
    const fileSelect = document.getElementById("fileSelect");
    const modeSelect = document.getElementById("modeSelect");
    const refreshBtn = document.getElementById("refreshBtn");
    const compileBtn = document.getElementById("compileBtn");
    const statusEl = document.getElementById("status");
    const sourceView = document.getElementById("sourceView");
    const tabsEl = document.getElementById("tabs");
    const contentView = document.getElementById("contentView");

    const state = { outputs: {}, activeKey: "" };

    function setStatus(text) {
      statusEl.textContent = text;
    }

    function renderTabs() {
      tabsEl.innerHTML = "";
      const keys = Object.keys(state.outputs);
      if (!keys.length) {
        contentView.textContent = "No results yet.";
        return;
      }
      if (!state.activeKey || !state.outputs[state.activeKey]) {
        state.activeKey = keys[0];
      }
      for (const key of keys) {
        const btn = document.createElement("button");
        btn.className = "tab" + (key === state.activeKey ? " active" : "");
        btn.textContent = key;
        btn.onclick = () => {
          state.activeKey = key;
          renderTabs();
        };
        tabsEl.appendChild(btn);
      }
      contentView.textContent = state.outputs[state.activeKey] || "";
    }

    async function loadFiles() {
      setStatus("Loading test files...");
      const res = await fetch("/api/files");
      const data = await res.json();
      fileSelect.innerHTML = "";
      for (const file of data.files || []) {
        const opt = document.createElement("option");
        opt.value = file;
        opt.textContent = file;
        fileSelect.appendChild(opt);
      }
      if ((data.files || []).length === 0) {
        const opt = document.createElement("option");
        opt.value = "";
        opt.textContent = "No .snl files found";
        fileSelect.appendChild(opt);
      }
      await loadSourcePreview();
      setStatus("File list loaded.");
    }

    async function loadSourcePreview() {
      const file = fileSelect.value;
      if (!file) {
        sourceView.textContent = "No file selected.";
        return;
      }
      sourceView.textContent = "Loading source...";
      try {
        const res = await fetch("/api/source?file=" + encodeURIComponent(file));
        const data = await res.json();
        if (!res.ok || !data.ok) {
          sourceView.textContent = "Failed to load source: " + (data.message || "Unknown error");
          return;
        }
        sourceView.textContent = data.content || "";
      } catch (err) {
        sourceView.textContent = "Request failed: " + err;
      }
    }

    async function compileSelected() {
      const file = fileSelect.value;
      const mode = modeSelect.value;
      if (!file) {
        setStatus("Please select a .snl file first.");
        return;
      }

      compileBtn.disabled = true;
      setStatus("Compiling...");
      try {
        const res = await fetch("/api/compile", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ file, mode })
        });
        const data = await res.json();
        if (!res.ok || !data.ok) {
          setStatus("Compile failed:\n" + (data.message || "Unknown error"));
        } else {
          setStatus(
            "Compile finished.\n" +
            "Input: " + (data.input || "") + "\n" +
            "Output dir: " + (data.output_dir || "") + "\n" +
            "Exit code: " + data.exit_code
          );
        }

        state.outputs = {};
        if (data.compiler_output) {
          state.outputs["compiler_output"] = data.compiler_output;
        }
        if (data.outputs) {
          for (const [k, v] of Object.entries(data.outputs)) {
            state.outputs[k] = v;
          }
        }
        renderTabs();
      } catch (err) {
        setStatus("Request failed: " + err);
      } finally {
        compileBtn.disabled = false;
      }
    }

    refreshBtn.onclick = () => loadFiles();
    fileSelect.onchange = () => loadSourcePreview();
    compileBtn.onclick = () => compileSelected();
    loadFiles();
  </script>
</body>
</html>
)HTML";
}

static std::wstring quoteCmdArg(const std::wstring& arg) {
    if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out = L"\"";
    for (wchar_t ch : arg) {
        if (ch == L'"') out += L'\\';
        out += ch;
    }
    out += L"\"";
    return out;
}

static bool runProcessCapture(const fs::path& exePath, const std::vector<std::wstring>& args, int& exitCode, std::string& output) {
    output.clear();
    exitCode = -1;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmd = quoteCmdArg(exePath.wstring());
    for (const auto& arg : args) {
        cmd += L" ";
        cmd += quoteCmdArg(arg);
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(
        nullptr,
        cmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        return false;
    }

    std::array<char, 4096> buf{};
    while (true) {
        DWORD bytesRead = 0;
        BOOL readOk = ReadFile(readPipe, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead, nullptr);
        if (!readOk || bytesRead == 0) break;
        output.append(buf.data(), bytesRead);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD procCode = 1;
    GetExitCodeProcess(pi.hProcess, &procCode);
    exitCode = static_cast<int>(procCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(readPipe);
    return true;
}

class WebUiServer {
public:
    explicit WebUiServer(int port, fs::path exePath) : port_(port) {
        fs::path cwdRoot = findProjectRoot(fs::current_path());
        fs::path exeRoot = findProjectRoot(exePath.parent_path());
        if (!cwdRoot.empty()) {
            projectRoot_ = cwdRoot;
        } else if (!exeRoot.empty()) {
            projectRoot_ = exeRoot;
        } else {
            projectRoot_ = fs::current_path();
        }
        testsRoot_ = projectRoot_ / "tests";
        compilerPath_ = detectCompiler(exePath.parent_path());
    }

    int run() {
        WSADATA wsaData{};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed\n";
            return 1;
        }

        SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSock == INVALID_SOCKET) {
            WSACleanup();
            std::cerr << "Failed to create socket\n";
            return 1;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<u_short>(port_));
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (bind(serverSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            closesocket(serverSock);
            WSACleanup();
            std::cerr << "Bind failed on port " << port_ << "\n";
            return 1;
        }

        if (listen(serverSock, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(serverSock);
            WSACleanup();
            std::cerr << "Listen failed\n";
            return 1;
        }

        std::cout << "SNL Web UI server started:\n";
        std::cout << "  URL: http://127.0.0.1:" << port_ << "\n";
        std::cout << "  Project Root: " << projectRoot_.string() << "\n";
        std::cout << "  Compiler: " << compilerPath_.string() << "\n";

        while (true) {
            SOCKET clientSock = accept(serverSock, nullptr, nullptr);
            if (clientSock == INVALID_SOCKET) {
                continue;
            }
            handleClient(clientSock);
            closesocket(clientSock);
        }
    }

private:
    int port_ = 8088;
    fs::path projectRoot_;
    fs::path testsRoot_;
    fs::path compilerPath_;

    fs::path detectCompiler(const fs::path& exeDir) const {
        std::vector<fs::path> candidates = {
            exeDir / "snlc.exe",
            projectRoot_ / "build" / "Debug" / "snlc.exe",
            projectRoot_ / "build" / "Release" / "snlc.exe",
            projectRoot_ / "snlc.exe"
        };
        for (const auto& c : candidates) {
            if (fs::exists(c)) return c;
        }
        return candidates.front();
    }

    std::vector<std::string> listSnlFiles() const {
        std::vector<std::string> files;
        if (!fs::exists(testsRoot_)) return files;
        for (auto it = fs::recursive_directory_iterator(testsRoot_); it != fs::recursive_directory_iterator(); ++it) {
            if (!it->is_regular_file()) continue;
            if (toLowerAscii(it->path().extension().string()) != ".snl") continue;
            fs::path rel = fs::relative(it->path(), projectRoot_);
            files.push_back(rel.generic_string());
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    bool modeValid(const std::string& mode) const {
        static const std::set<std::string> modes = {"lex", "rd", "ll1", "sem", "all"};
        return modes.count(mode) > 0;
    }

    bool resolveSnlPath(const std::string& relFile, fs::path& inputPath, std::string& message) const {
        fs::path requested = fs::path(relFile);
        if (requested.empty()) {
            message = "File is required.";
            return false;
        }
        if (requested.is_absolute()) {
            message = "Absolute path is not allowed.";
            return false;
        }
        fs::path testsCanon = fs::weakly_canonical(testsRoot_);
        inputPath = fs::weakly_canonical(projectRoot_ / requested);
        if (!fs::exists(inputPath)) {
            message = "Input file not found.";
            return false;
        }
        if (toLowerAscii(inputPath.extension().string()) != ".snl") {
            message = "Only .snl files are allowed.";
            return false;
        }
        if (!pathStartsWith(inputPath, testsCanon)) {
            message = "Only files under tests/ are allowed.";
            return false;
        }
        return true;
    }

    CompileResult compileFile(const std::string& relFile, const std::string& mode) const {
        CompileResult res;
        if (!modeValid(mode)) {
            res.message = "Invalid mode: " + mode;
            return res;
        }

        fs::path inputPath;
        if (!resolveSnlPath(relFile, inputPath, res.message)) {
            return res;
        }
        if (!fs::exists(compilerPath_)) {
            res.message = "Compiler not found: " + compilerPath_.string();
            return res;
        }

        res.requestOk = true;
        res.inputPath = inputPath;
        res.outputDir = inputPath.parent_path() / (inputPath.stem().string() + "_outputs");

        std::vector<std::wstring> args = {
            L"--input", inputPath.wstring(),
            L"--mode", std::wstring(mode.begin(), mode.end())
        };
        int status = -1;
        if (!runProcessCapture(compilerPath_, args, status, res.compilerOutput)) {
            res.message = "Failed to start compiler process.";
            return res;
        }
        res.exitCode = status;
        res.compileOk = (status == 0);
        if (!res.compileOk && res.message.empty()) {
            res.message = "Compiler returned non-zero exit code.";
        }

        const std::string base = inputPath.stem().string();
        const std::vector<std::pair<std::string, fs::path>> outputFiles = {
            {"errors", res.outputDir / (base + ".errors.txt")},
            {"symbols", res.outputDir / (base + ".symbols.txt")},
            {"tokens", res.outputDir / (base + ".tokens.txt")},
            {"rd_tree", res.outputDir / (base + ".rd_tree.txt")},
            {"ll1_tree", res.outputDir / (base + ".ll1_tree.txt")},
            {"asm", res.outputDir / (base + ".asm")}
        };

        for (const auto& kv : outputFiles) {
            std::string content;
            if (readFileText(kv.second, content)) {
                res.outputs[kv.first] = std::move(content);
            }
        }

        return res;
    }

    std::string buildFilesJson() const {
        auto files = listSnlFiles();
        std::ostringstream oss;
        oss << "{\"files\":[";
        for (size_t i = 0; i < files.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << jsonEscape(files[i]) << "\"";
        }
        oss << "]}";
        return oss.str();
    }

    std::string buildCompileJson(const CompileResult& r) const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"ok\":" << (r.compileOk ? "true" : "false") << ",";
        oss << "\"request_ok\":" << (r.requestOk ? "true" : "false") << ",";
        oss << "\"exit_code\":" << r.exitCode << ",";
        oss << "\"message\":\"" << jsonEscape(r.message) << "\",";
        oss << "\"input\":\"" << jsonEscape(r.inputPath.generic_string()) << "\",";
        oss << "\"output_dir\":\"" << jsonEscape(r.outputDir.generic_string()) << "\",";
        oss << "\"compiler_output\":\"" << jsonEscape(r.compilerOutput) << "\",";
        oss << "\"outputs\":{";
        bool first = true;
        for (const auto& kv : r.outputs) {
            if (!first) oss << ",";
            first = false;
            oss << "\"" << jsonEscape(kv.first) << "\":\"" << jsonEscape(kv.second) << "\"";
        }
        oss << "}";
        oss << "}";
        return oss.str();
    }

    std::string buildSourceJson(bool ok, const std::string& message, const std::string& file, const std::string& content) const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"ok\":" << (ok ? "true" : "false") << ",";
        oss << "\"message\":\"" << jsonEscape(message) << "\",";
        oss << "\"file\":\"" << jsonEscape(file) << "\",";
        oss << "\"content\":\"" << jsonEscape(content) << "\"";
        oss << "}";
        return oss.str();
    }

    void handleClient(SOCKET clientSock) {
        HttpRequest req;
        if (!parseRequest(clientSock, req)) {
            sendResponse(clientSock, 400, "text/plain", "Bad Request");
            return;
        }

        std::string path = req.path;
        std::string query;
        size_t q = path.find('?');
        if (q != std::string::npos) {
            query = path.substr(q + 1);
            path = path.substr(0, q);
        }

        if (req.method == "GET" && path == "/") {
            sendResponse(clientSock, 200, "text/html", buildIndexHtml());
            return;
        }
        if (req.method == "GET" && path == "/api/files") {
            sendResponse(clientSock, 200, "application/json", buildFilesJson());
            return;
        }
        if (req.method == "GET" && path == "/api/source") {
            std::string file = getQueryParam(query, "file");
            fs::path inputPath;
            std::string message;
            if (!resolveSnlPath(file, inputPath, message)) {
                sendResponse(clientSock, 400, "application/json", buildSourceJson(false, message, file, ""));
                return;
            }
            std::string content;
            if (!readFileText(inputPath, content)) {
                sendResponse(clientSock, 500, "application/json", buildSourceJson(false, "Failed to read file.", file, ""));
                return;
            }
            sendResponse(
                clientSock,
                200,
                "application/json",
                buildSourceJson(true, "", fs::relative(inputPath, projectRoot_).generic_string(), content)
            );
            return;
        }
        if (req.method == "POST" && path == "/api/compile") {
            std::string file = parseJsonStringField(req.body, "file");
            std::string mode = parseJsonStringField(req.body, "mode");
            CompileResult cr = compileFile(file, mode.empty() ? "all" : mode);
            int status = cr.requestOk ? 200 : 400;
            sendResponse(clientSock, status, "application/json", buildCompileJson(cr));
            return;
        }
        if (req.method == "GET" && path == "/favicon.ico") {
            sendResponse(clientSock, 204, "text/plain", "");
            return;
        }

        if (path == "/api/compile" || path == "/api/files" || path == "/api/source" || path == "/") {
            sendResponse(clientSock, 405, "text/plain", "Method Not Allowed");
        } else {
            sendResponse(clientSock, 404, "text/plain", "Not Found");
        }
    }
};

int main(int argc, char** argv) {
    int port = 8088;
    if (argc >= 2) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            port = 8088;
        }
    }

    fs::path exePath = fs::absolute(argv[0]);
    WebUiServer server(port, exePath);
    return server.run();
}
