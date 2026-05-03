/* =========================================================
   OpenWebUI-like Interface – app.js
   ========================================================= */

'use strict';

// ---- State ----------------------------------------------------------------

const state = {
  conversations: [],
  currentId: null,
  currentConv: null,
  messages: [],        // {id, role, content, images}
  models: [],
  selectedModel: '',
  settings: {},
  isGenerating: false,
  abortController: null,
  pendingFiles: [],    // File objects waiting to be sent
  renameTargetId: null,
  theme: 'dark',
};

// ---- DOM refs -------------------------------------------------------------

const $ = id => document.getElementById(id);

const els = {
  sidebar:            $('sidebar'),
  sidebarToggle:      $('sidebar-toggle-main'),
  newChatBtn:         $('new-chat-btn'),
  searchInput:        $('search-input'),
  convList:           $('conversations-list'),
  settingsBtn:        $('settings-btn'),
  themeBtn:           $('theme-btn'),
  themeIcon:          $('theme-icon'),
  modelSelect:        $('model-select'),
  providerBadge:      $('provider-badge'),
  refreshModels:      $('refresh-models-btn'),
  systemPromptBtn:    $('system-prompt-btn'),
  deleteConvBtn:      $('delete-conv-btn'),
  messagesContainer:  $('messages-container'),
  messagesInner:      $('messages-inner'),
  welcomeScreen:      $('welcome-screen'),
  welcomeModels:      $('welcome-models'),
  filePreviews:       $('file-previews'),
  messageInput:       $('message-input'),
  attachBtn:          $('attach-btn'),
  fileInput:          $('file-input'),
  sendBtn:            $('send-btn'),
  stopBtn:            $('stop-btn'),
  modelInfoLabel:     $('model-info-label'),

  // Settings modal
  settingsOverlay:    $('settings-overlay'),
  settingTheme:       $('setting-theme'),
  settingDefaultModel:$('setting-default-model'),
  settingOllamaUrl:   $('setting-ollama-url'),
  settingOpenaiKey:   $('setting-openai-key'),
  settingOpenaiUrl:   $('setting-openai-url'),
  settingAnthropicKey:$('setting-anthropic-key'),
  settingTemperature: $('setting-temperature'),
  settingTopP:        $('setting-top-p'),
  settingMaxTokens:   $('setting-max-tokens'),
  tempDisplay:        $('temp-display'),
  toppDisplay:        $('topp-display'),
  saveSettingsBtn:    $('save-settings-btn'),

  // System prompt modal
  systemPromptOverlay:$('system-prompt-overlay'),
  systemPromptInput:  $('system-prompt-input'),
  saveSystemPromptBtn:$('save-system-prompt-btn'),

  // Rename modal
  renameOverlay:      $('rename-overlay'),
  renameInput:        $('rename-input'),
  renameConfirmBtn:   $('rename-confirm-btn'),

  toasts:             $('toasts'),
};

// ---- Helpers --------------------------------------------------------------

function formatTime(iso) {
  if (!iso) return '';
  const d = new Date(iso + (iso.endsWith('Z') ? '' : 'Z'));
  return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

function escapeHtml(str) {
  return str
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function detectProvider(modelId) {
  if (!modelId) return 'ollama';
  const m = modelId.toLowerCase();
  if (m.includes('claude')) return 'anthropic';
  if (m.match(/gpt-|o1-|o3-|o4-/)) return 'openai';
  return 'ollama';
}

function renderMarkdown(text) {
  if (typeof marked === 'undefined') return escapeHtml(text);
  try {
    marked.setOptions({ breaks: true, gfm: true });
    const raw = marked.parse(text);
    // Wrap fenced code blocks in our custom container
    return raw.replace(
      /<pre><code(?: class="language-([^"]*)")?>([\s\S]*?)<\/code><\/pre>/g,
      (_, lang, code) => {
        const label = lang || 'code';
        return `<div class="code-block">
          <div class="code-block-header">
            <span class="code-lang">${escapeHtml(label)}</span>
            <button class="copy-code-btn" onclick="copyCode(this)">Copy</button>
          </div>
          <pre><code class="language-${escapeHtml(label || 'plaintext')}">${code}</code></pre>
        </div>`;
      }
    );
  } catch {
    return escapeHtml(text);
  }
}

function highlightAll() {
  if (typeof hljs !== 'undefined') {
    document.querySelectorAll('.code-block pre code').forEach(el => {
      if (!el.dataset.highlighted) hljs.highlightElement(el);
    });
  }
}

window.copyCode = function(btn) {
  const code = btn.closest('.code-block').querySelector('code');
  navigator.clipboard.writeText(code.innerText).then(() => {
    btn.textContent = 'Copied!';
    btn.classList.add('copied');
    setTimeout(() => { btn.textContent = 'Copy'; btn.classList.remove('copied'); }, 1500);
  });
};

function scrollToBottom(force = false) {
  const c = els.messagesContainer;
  const threshold = 100;
  const near = c.scrollHeight - c.scrollTop - c.clientHeight < threshold;
  if (near || force) {
    c.scrollTop = c.scrollHeight;
  }
}

// ---- Toast ----------------------------------------------------------------

function toast(message, type = 'info', duration = 3000) {
  const el = document.createElement('div');
  el.className = `toast ${type}`;
  el.textContent = message;
  els.toasts.appendChild(el);
  setTimeout(() => el.remove(), duration);
}

// ---- Theme ----------------------------------------------------------------

function applyTheme(theme) {
  state.theme = theme || state.theme;
  if (state.theme === 'light') {
    document.body.classList.add('theme-light');
    // Switch hljs theme to light
    const link = document.getElementById('hljs-theme');
    if (link) link.href = 'https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github.min.css';
    els.themeIcon.innerHTML = `<circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/>`;
  } else {
    document.body.classList.remove('theme-light');
    const link = document.getElementById('hljs-theme');
    if (link) link.href = 'https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/atom-one-dark.min.css';
    els.themeIcon.innerHTML = `<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>`;
  }
}

// ---- API ------------------------------------------------------------------

async function api(path, options = {}) {
  const res = await fetch(path, {
    headers: { 'Content-Type': 'application/json', ...options.headers },
    ...options,
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  if (res.status === 204) return null;
  return res.json();
}

// ---- Models ---------------------------------------------------------------

async function loadModels() {
  try {
    const data = await api('/api/models');
    state.models = data.models || [];
  } catch {
    state.models = [];
  }
  renderModelSelect();
  renderWelcomeModels();
}

function renderModelSelect() {
  const sel = els.modelSelect;
  const prev = sel.value;
  sel.innerHTML = '<option value="">— Select a model —</option>';

  const groups = { ollama: [], openai: [], anthropic: [] };
  state.models.forEach(m => (groups[m.provider] || groups.ollama).push(m));

  const labels = { ollama: 'Ollama (Local)', openai: 'OpenAI', anthropic: 'Anthropic' };
  for (const [provider, models] of Object.entries(groups)) {
    if (!models.length) continue;
    const og = document.createElement('optgroup');
    og.label = labels[provider];
    models.forEach(m => {
      const opt = document.createElement('option');
      opt.value = m.id;
      opt.textContent = m.name;
      og.appendChild(opt);
    });
    sel.appendChild(og);
  }

  // Restore selection
  const target = prev || state.settings.default_model || '';
  if (target) sel.value = target;
  if (sel.value) {
    state.selectedModel = sel.value;
    updateProviderBadge(sel.value);
    updateModelInfoLabel();
  }

  // Populate settings default model
  const defSel = els.settingDefaultModel;
  defSel.innerHTML = '<option value="">None</option>';
  state.models.forEach(m => {
    const opt = document.createElement('option');
    opt.value = m.id;
    opt.textContent = m.name;
    defSel.appendChild(opt);
  });
  defSel.value = state.settings.default_model || '';
}

function updateProviderBadge(modelId) {
  const provider = detectProvider(modelId);
  const badge = els.providerBadge;
  if (!modelId) { badge.classList.add('hidden'); return; }
  badge.classList.remove('hidden', 'ollama', 'openai', 'anthropic');
  badge.classList.add(provider);
  badge.textContent = provider;
}

function updateModelInfoLabel() {
  const m = state.models.find(x => x.id === state.selectedModel);
  if (m) {
    const size = m.size ? ` · ${(m.size / 1e9).toFixed(1)}B` : '';
    els.modelInfoLabel.textContent = m.name + size;
  } else {
    els.modelInfoLabel.textContent = state.selectedModel || '';
  }
}

function renderWelcomeModels() {
  const container = els.welcomeModels;
  container.innerHTML = '';
  state.models.slice(0, 8).forEach(m => {
    const chip = document.createElement('button');
    chip.className = 'welcome-model-chip';
    chip.textContent = m.name;
    chip.addEventListener('click', () => {
      els.modelSelect.value = m.id;
      state.selectedModel = m.id;
      updateProviderBadge(m.id);
      updateModelInfoLabel();
      els.messageInput.focus();
    });
    container.appendChild(chip);
  });
}

// ---- Settings -------------------------------------------------------------

async function loadSettings() {
  try {
    state.settings = await api('/api/settings');
  } catch {
    state.settings = {};
  }
  applySettingsToUI();
  applyTheme(state.settings.theme || 'dark');
}

function applySettingsToUI() {
  const s = state.settings;
  els.settingTheme.value        = s.theme || 'dark';
  els.settingOllamaUrl.value    = s.ollama_url || 'http://localhost:11434';
  els.settingOpenaiUrl.value    = s.openai_base_url || 'https://api.openai.com/v1';
  els.settingTemperature.value  = s.temperature || '0.7';
  els.settingTopP.value         = s.top_p || '0.9';
  els.settingMaxTokens.value    = s.max_tokens || '4096';
  els.tempDisplay.textContent   = s.temperature || '0.7';
  els.toppDisplay.textContent   = s.top_p || '0.9';
  // API keys are server-masked; show placeholder if set
  if (s.openai_api_key_masked)   els.settingOpenaiKey.placeholder = s.openai_api_key_masked;
  if (s.anthropic_api_key_masked) els.settingAnthropicKey.placeholder = s.anthropic_api_key_masked;
}

async function saveSettings() {
  const payload = {
    theme:           els.settingTheme.value,
    ollama_url:      els.settingOllamaUrl.value.trim(),
    openai_base_url: els.settingOpenaiUrl.value.trim(),
    temperature:     els.settingTemperature.value,
    top_p:           els.settingTopP.value,
    max_tokens:      els.settingMaxTokens.value,
    default_model:   els.settingDefaultModel.value,
  };
  const openaiKey = els.settingOpenaiKey.value.trim();
  if (openaiKey) payload.openai_api_key = openaiKey;
  const anthropicKey = els.settingAnthropicKey.value.trim();
  if (anthropicKey) payload.anthropic_api_key = anthropicKey;

  await api('/api/settings', { method: 'PUT', body: JSON.stringify(payload) });
  state.settings = { ...state.settings, ...payload };
  applyTheme(payload.theme);
  closeModal('settings-overlay');
  await loadModels();
  toast('Settings saved', 'success');
}

// ---- Conversations --------------------------------------------------------

async function loadConversations() {
  try {
    state.conversations = await api('/api/conversations');
  } catch {
    state.conversations = [];
  }
  renderConversationList();
}

function groupConversations(convs) {
  const now = new Date();
  const today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  const yesterday = new Date(today - 86400000);
  const weekAgo = new Date(today - 7 * 86400000);

  const groups = { Today: [], Yesterday: [], 'This Week': [], Older: [] };
  convs.forEach(c => {
    const d = new Date(c.updated_at + 'Z');
    if (d >= today) groups['Today'].push(c);
    else if (d >= yesterday) groups['Yesterday'].push(c);
    else if (d >= weekAgo) groups['This Week'].push(c);
    else groups['Older'].push(c);
  });
  return groups;
}

function renderConversationList(filter = '') {
  const container = els.convList;
  container.innerHTML = '';

  const convs = filter
    ? state.conversations.filter(c => c.title.toLowerCase().includes(filter.toLowerCase()))
    : state.conversations;

  if (!convs.length) {
    container.innerHTML = '<div class="empty-state">No conversations yet.<br>Start a new chat!</div>';
    return;
  }

  const groups = groupConversations(convs);
  for (const [label, items] of Object.entries(groups)) {
    if (!items.length) continue;
    const groupLabel = document.createElement('div');
    groupLabel.className = 'conv-group-label';
    groupLabel.textContent = label;
    container.appendChild(groupLabel);
    items.forEach(c => container.appendChild(buildConvItem(c)));
  }
}

function buildConvItem(conv) {
  const div = document.createElement('div');
  div.className = 'conv-item' + (conv.id === state.currentId ? ' active' : '');
  div.dataset.id = conv.id;
  div.title = conv.title;

  div.innerHTML = `
    <svg class="conv-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
      <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>
    </svg>
    <span class="conv-title">${escapeHtml(conv.title)}</span>
    <div class="conv-actions">
      <button class="conv-action-btn" title="Rename" data-action="rename" data-id="${conv.id}">
        <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/>
          <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/>
        </svg>
      </button>
      <button class="conv-action-btn danger" title="Delete" data-action="delete" data-id="${conv.id}">
        <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/>
        </svg>
      </button>
    </div>`;

  div.addEventListener('click', e => {
    const action = e.target.closest('[data-action]');
    if (action) {
      e.stopPropagation();
      if (action.dataset.action === 'rename') openRenameModal(action.dataset.id);
      if (action.dataset.action === 'delete') confirmDeleteConv(action.dataset.id);
    } else {
      openConversation(conv.id);
    }
  });

  return div;
}

async function openConversation(id) {
  if (state.isGenerating) return;
  state.currentId = id;

  try {
    const data = await api(`/api/conversations/${id}`);
    state.currentConv = data;
    state.messages = data.messages || [];
    renderMessages();

    // Update model selector to match conversation's model
    if (data.model && data.model !== els.modelSelect.value) {
      els.modelSelect.value = data.model;
      state.selectedModel = data.model;
      updateProviderBadge(data.model);
      updateModelInfoLabel();
    }

    // Highlight active in sidebar
    document.querySelectorAll('.conv-item').forEach(el => {
      el.classList.toggle('active', el.dataset.id === id);
    });

    els.welcomeScreen.classList.add('hidden');
    els.messageInput.focus();
  } catch (e) {
    toast('Failed to load conversation', 'error');
  }
}

async function createNewConversation(title, model) {
  const data = await api('/api/conversations', {
    method: 'POST',
    body: JSON.stringify({ title: title || 'New Chat', model: model || state.selectedModel }),
  });
  state.conversations.unshift(data);
  state.currentId = data.id;
  state.currentConv = data;
  state.messages = [];
  renderConversationList();
  document.querySelectorAll('.conv-item').forEach(el => {
    el.classList.toggle('active', el.dataset.id === data.id);
  });
  return data;
}

async function deleteConversation(id) {
  await api(`/api/conversations/${id}`, { method: 'DELETE' });
  state.conversations = state.conversations.filter(c => c.id !== id);
  if (state.currentId === id) {
    state.currentId = null;
    state.currentConv = null;
    state.messages = [];
    renderMessages();
    els.welcomeScreen.classList.remove('hidden');
  }
  renderConversationList();
  toast('Conversation deleted', 'info');
}

function confirmDeleteConv(id) {
  if (confirm('Delete this conversation? This cannot be undone.')) {
    deleteConversation(id);
  }
}

function openRenameModal(id) {
  state.renameTargetId = id;
  const conv = state.conversations.find(c => c.id === id);
  els.renameInput.value = conv ? conv.title : '';
  openModal('rename-overlay');
  setTimeout(() => { els.renameInput.focus(); els.renameInput.select(); }, 80);
}

async function renameConversation(id, title) {
  const data = await api(`/api/conversations/${id}`, {
    method: 'PUT',
    body: JSON.stringify({ title }),
  });
  const idx = state.conversations.findIndex(c => c.id === id);
  if (idx !== -1) state.conversations[idx].title = data.title;
  if (state.currentId === id && state.currentConv) state.currentConv.title = data.title;
  renderConversationList();
  closeModal('rename-overlay');
}

// ---- Render messages ------------------------------------------------------

function renderMessages() {
  const inner = els.messagesInner;
  // Clear existing messages but keep welcome screen (it has its own visibility)
  inner.querySelectorAll('.message').forEach(el => el.remove());

  if (!state.messages.length) {
    els.welcomeScreen.classList.remove('hidden');
    return;
  }

  els.welcomeScreen.classList.add('hidden');
  state.messages.forEach(msg => inner.appendChild(buildMessageEl(msg)));
  highlightAll();
  scrollToBottom(true);
}

function buildMessageEl(msg) {
  const isUser = msg.role === 'user';
  const div = document.createElement('div');
  div.className = `message ${msg.role}`;
  div.dataset.id = msg.id || '';

  const avatarChar = isUser ? 'U' : '🤖';
  const roleLabel  = isUser ? 'You' : (state.selectedModel || 'Assistant');
  const timeStr    = formatTime(msg.created_at);

  const contentHtml = isUser
    ? escapeHtml(msg.content)
    : renderMarkdown(msg.content || '');

  // Image previews
  let imagesHtml = '';
  const images = typeof msg.images === 'string' ? JSON.parse(msg.images || '[]') : (msg.images || []);
  if (images.length) {
    imagesHtml = `<div class="file-previews" style="margin-bottom:8px">` +
      images.map(url => `<div class="file-preview"><img src="${escapeHtml(url)}" alt="attached" /></div>`).join('') +
      `</div>`;
  }

  div.innerHTML = `
    <div class="avatar ${isUser ? 'user-avatar' : 'assistant-avatar'}">${avatarChar}</div>
    <div class="message-body">
      <div class="message-meta">
        <span class="message-role">${escapeHtml(roleLabel)}</span>
        <span class="message-time">${timeStr}</span>
      </div>
      ${imagesHtml}
      <div class="message-content">${contentHtml}</div>
      <div class="message-actions">
        <button class="msg-action-btn" onclick="copyMessage(this)" title="Copy">
          <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="9" y="9" width="13" height="13" rx="2" ry="2"/>
            <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>
          </svg>
          Copy
        </button>
      </div>
    </div>`;

  return div;
}

window.copyMessage = function(btn) {
  const content = btn.closest('.message').querySelector('.message-content');
  navigator.clipboard.writeText(content.innerText).then(() => {
    const orig = btn.innerHTML;
    btn.textContent = 'Copied!';
    setTimeout(() => { btn.innerHTML = orig; }, 1500);
  });
};

// ---- Chat -----------------------------------------------------------------

async function sendMessage() {
  const text = els.messageInput.value.trim();
  if (!text && !state.pendingFiles.length) return;
  if (state.isGenerating) return;

  if (!state.selectedModel) {
    toast('Please select a model first', 'error');
    return;
  }

  els.messageInput.value = '';
  autoResizeTextarea();

  // Upload pending files first
  const imageUrls = [];
  for (const file of state.pendingFiles) {
    try {
      const form = new FormData();
      form.append('file', file);
      const res = await fetch('/api/upload', { method: 'POST', body: form });
      const data = await res.json();
      imageUrls.push(data.url);
    } catch {
      toast(`Failed to upload ${file.name}`, 'error');
    }
  }
  clearFilePreviews();

  // Ensure conversation exists
  if (!state.currentConv) {
    await createNewConversation('New Chat', state.selectedModel);
  }

  // Save & display user message
  const userMsg = { role: 'user', content: text, images: imageUrls, created_at: new Date().toISOString() };
  state.messages.push(userMsg);
  els.welcomeScreen.classList.add('hidden');
  els.messagesInner.appendChild(buildMessageEl(userMsg));
  scrollToBottom(true);

  // Persist user message
  try {
    const saved = await api(`/api/conversations/${state.currentConv.id}/messages`, {
      method: 'POST',
      body: JSON.stringify({ role: 'user', content: text, images: imageUrls }),
    });
    userMsg.id = saved.id;
  } catch { /* non-critical */ }

  // Update conversation model if changed
  if (state.currentConv.model !== state.selectedModel) {
    await api(`/api/conversations/${state.currentConv.id}`, {
      method: 'PUT',
      body: JSON.stringify({ model: state.selectedModel }),
    });
    state.currentConv.model = state.selectedModel;
  }

  // Stream AI response
  await streamResponse(text);

  // Auto-title after first exchange
  if (state.messages.filter(m => m.role === 'user').length === 1) {
    autoTitleConversation(text);
  }
}

async function streamResponse(userContent) {
  state.isGenerating = true;
  els.sendBtn.classList.add('hidden');
  els.stopBtn.classList.remove('hidden');
  els.sendBtn.disabled = true;

  // Build messages for API (last 40 to stay within context)
  const apiMessages = state.messages
    .slice(-40)
    .map(m => ({ role: m.role, content: m.content }));

  // Create assistant placeholder
  const assistantMsg = {
    role: 'assistant',
    content: '',
    created_at: new Date().toISOString(),
  };
  state.messages.push(assistantMsg);
  const msgEl = buildMessageEl(assistantMsg);
  // Add typing indicator
  const contentEl = msgEl.querySelector('.message-content');
  contentEl.innerHTML = `<div class="typing-indicator">
    <div class="typing-dot"></div><div class="typing-dot"></div><div class="typing-dot"></div>
  </div>`;
  els.messagesInner.appendChild(msgEl);
  scrollToBottom(true);

  state.abortController = new AbortController();

  try {
    const response = await fetch('/api/chat', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        model: state.selectedModel,
        provider: detectProvider(state.selectedModel),
        messages: apiMessages,
        system_prompt: state.currentConv?.system_prompt || '',
        stream: true,
      }),
      signal: state.abortController.signal,
    });

    if (!response.ok) {
      const err = await response.text();
      throw new Error(err || `HTTP ${response.status}`);
    }

    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let buffer = '';
    let fullContent = '';

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;

      buffer += decoder.decode(value, { stream: true });
      const lines = buffer.split('\n');
      buffer = lines.pop(); // keep incomplete last line

      for (const line of lines) {
        if (!line.startsWith('data: ')) continue;
        const raw = line.slice(6).trim();
        if (!raw) continue;
        try {
          const chunk = JSON.parse(raw);
          if (chunk.error) {
            throw new Error(chunk.error);
          }
          if (chunk.content) {
            fullContent += chunk.content;
            contentEl.innerHTML = renderMarkdown(fullContent);
            highlightAll();
            scrollToBottom();
          }
          if (chunk.done) {
            break;
          }
        } catch (parseErr) {
          if (parseErr.message !== 'Unexpected end of JSON input') {
            throw parseErr;
          }
        }
      }
    }

    // Final render
    assistantMsg.content = fullContent;
    contentEl.innerHTML = renderMarkdown(fullContent || '*(no response)*');
    highlightAll();
    scrollToBottom(true);

    // Persist assistant message
    try {
      const saved = await api(`/api/conversations/${state.currentConv.id}/messages`, {
        method: 'POST',
        body: JSON.stringify({ role: 'assistant', content: fullContent }),
      });
      assistantMsg.id = saved.id;
    } catch { /* non-critical */ }

  } catch (err) {
    if (err.name === 'AbortError') {
      contentEl.innerHTML += '<em style="color:var(--text-muted)"> [stopped]</em>';
    } else {
      contentEl.innerHTML = `<span style="color:var(--danger)">Error: ${escapeHtml(err.message)}</span>`;
      toast(`Error: ${err.message}`, 'error', 5000);
    }
  } finally {
    state.isGenerating = false;
    state.abortController = null;
    els.sendBtn.classList.remove('hidden');
    els.stopBtn.classList.add('hidden');
    els.sendBtn.disabled = false;
    els.messageInput.focus();

    // Refresh sidebar to update updated_at order
    await loadConversations();
  }
}

async function autoTitleConversation(firstMessage) {
  if (!state.currentConv) return;
  try {
    const data = await api('/api/generate-title', {
      method: 'POST',
      body: JSON.stringify({ message: firstMessage }),
    });
    const title = data.title;
    await api(`/api/conversations/${state.currentConv.id}`, {
      method: 'PUT',
      body: JSON.stringify({ title }),
    });
    state.currentConv.title = title;
    const idx = state.conversations.findIndex(c => c.id === state.currentConv.id);
    if (idx !== -1) state.conversations[idx].title = title;
    renderConversationList();
  } catch { /* non-critical */ }
}

// ---- File attachments -----------------------------------------------------

function clearFilePreviews() {
  state.pendingFiles = [];
  els.filePreviews.innerHTML = '';
  els.filePreviews.classList.add('hidden');
}

function addFilePreview(file) {
  els.filePreviews.classList.remove('hidden');
  const div = document.createElement('div');
  div.className = 'file-preview';
  const url = URL.createObjectURL(file);
  div.innerHTML = `<img src="${url}" alt="${escapeHtml(file.name)}" /><span class="remove-file" title="Remove">✕</span>`;
  div.querySelector('.remove-file').addEventListener('click', () => {
    const idx = state.pendingFiles.indexOf(file);
    if (idx !== -1) state.pendingFiles.splice(idx, 1);
    URL.revokeObjectURL(url);
    div.remove();
    if (!state.pendingFiles.length) els.filePreviews.classList.add('hidden');
  });
  els.filePreviews.appendChild(div);
}

// ---- Textarea auto-resize -------------------------------------------------

function autoResizeTextarea() {
  const ta = els.messageInput;
  ta.style.height = 'auto';
  ta.style.height = Math.min(ta.scrollHeight, 200) + 'px';
}

// ---- Modal helpers --------------------------------------------------------

function openModal(id) {
  $(id).classList.remove('hidden');
}

function closeModal(id) {
  $(id).classList.add('hidden');
}

// ---- Event Listeners ------------------------------------------------------

function setupEvents() {
  // Sidebar toggle
  els.sidebarToggle.addEventListener('click', () => {
    els.sidebar.classList.toggle('collapsed');
  });

  // New chat
  els.newChatBtn.addEventListener('click', async () => {
    if (state.isGenerating) return;
    state.currentId = null;
    state.currentConv = null;
    state.messages = [];
    renderMessages();
    els.welcomeScreen.classList.remove('hidden');
    document.querySelectorAll('.conv-item').forEach(el => el.classList.remove('active'));
    els.messageInput.focus();
  });

  // Search
  els.searchInput.addEventListener('input', e => {
    renderConversationList(e.target.value);
  });

  // Model select
  els.modelSelect.addEventListener('change', () => {
    state.selectedModel = els.modelSelect.value;
    updateProviderBadge(state.selectedModel);
    updateModelInfoLabel();
  });

  // Refresh models
  els.refreshModels.addEventListener('click', async () => {
    els.refreshModels.style.opacity = '0.5';
    await loadModels();
    els.refreshModels.style.opacity = '1';
    toast('Models refreshed', 'success');
  });

  // Theme toggle
  els.themeBtn.addEventListener('click', () => {
    const newTheme = state.theme === 'dark' ? 'light' : 'dark';
    applyTheme(newTheme);
    api('/api/settings', { method: 'PUT', body: JSON.stringify({ theme: newTheme }) });
  });

  // Settings
  els.settingsBtn.addEventListener('click', () => openModal('settings-overlay'));

  // Settings tabs
  document.querySelectorAll('.settings-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.settings-tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.settings-tab-content').forEach(c => c.classList.remove('active'));
      tab.classList.add('active');
      $(tab.dataset.tab).classList.add('active');
    });
  });

  // Range sliders live display
  els.settingTemperature.addEventListener('input', () => {
    els.tempDisplay.textContent = els.settingTemperature.value;
  });
  els.settingTopP.addEventListener('input', () => {
    els.toppDisplay.textContent = els.settingTopP.value;
  });

  // Save settings
  els.saveSettingsBtn.addEventListener('click', saveSettings);

  // System prompt
  els.systemPromptBtn.addEventListener('click', () => {
    els.systemPromptInput.value = state.currentConv?.system_prompt || '';
    openModal('system-prompt-overlay');
    setTimeout(() => els.systemPromptInput.focus(), 80);
  });

  els.saveSystemPromptBtn.addEventListener('click', async () => {
    const sp = els.systemPromptInput.value.trim();
    if (state.currentConv) {
      await api(`/api/conversations/${state.currentConv.id}`, {
        method: 'PUT',
        body: JSON.stringify({ system_prompt: sp }),
      });
      state.currentConv.system_prompt = sp;
      toast('System prompt saved', 'success');
    }
    closeModal('system-prompt-overlay');
  });

  // Delete conversation
  els.deleteConvBtn.addEventListener('click', () => {
    if (state.currentId) confirmDeleteConv(state.currentId);
  });

  // Close modals
  document.querySelectorAll('.modal-close').forEach(btn => {
    btn.addEventListener('click', () => {
      if (btn.dataset.modal) closeModal(btn.dataset.modal);
    });
  });

  // Close modals on overlay click
  document.querySelectorAll('.modal-overlay').forEach(overlay => {
    overlay.addEventListener('click', e => {
      if (e.target === overlay) closeModal(overlay.id);
    });
  });

  // Rename confirm
  els.renameConfirmBtn.addEventListener('click', () => {
    const title = els.renameInput.value.trim();
    if (title && state.renameTargetId) {
      renameConversation(state.renameTargetId, title);
    }
  });

  els.renameInput.addEventListener('keydown', e => {
    if (e.key === 'Enter') {
      const title = els.renameInput.value.trim();
      if (title && state.renameTargetId) renameConversation(state.renameTargetId, title);
    }
    if (e.key === 'Escape') closeModal('rename-overlay');
  });

  // Message input
  els.messageInput.addEventListener('input', autoResizeTextarea);

  els.messageInput.addEventListener('keydown', e => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      sendMessage();
    }
  });

  // Send / Stop buttons
  els.sendBtn.addEventListener('click', sendMessage);
  els.stopBtn.addEventListener('click', () => {
    if (state.abortController) state.abortController.abort();
  });

  // File attach
  els.attachBtn.addEventListener('click', () => els.fileInput.click());
  els.fileInput.addEventListener('change', e => {
    Array.from(e.target.files).forEach(file => {
      if (file.type.startsWith('image/')) {
        state.pendingFiles.push(file);
        addFilePreview(file);
      }
    });
    e.target.value = '';
  });

  // Drag & drop files onto chat
  els.messagesContainer.addEventListener('dragover', e => e.preventDefault());
  els.messagesContainer.addEventListener('drop', e => {
    e.preventDefault();
    Array.from(e.dataTransfer.files).forEach(file => {
      if (file.type.startsWith('image/')) {
        state.pendingFiles.push(file);
        addFilePreview(file);
      }
    });
  });

  // Keyboard shortcuts
  document.addEventListener('keydown', e => {
    if ((e.ctrlKey || e.metaKey) && e.shiftKey && e.key === 'N') {
      e.preventDefault();
      els.newChatBtn.click();
    }
    if (e.key === 'Escape') {
      document.querySelectorAll('.modal-overlay:not(.hidden)').forEach(el => closeModal(el.id));
    }
  });
}

// ---- Init -----------------------------------------------------------------

async function init() {
  await loadSettings();
  await loadModels();
  await loadConversations();
  setupEvents();
  els.messageInput.focus();
}

document.addEventListener('DOMContentLoaded', init);
