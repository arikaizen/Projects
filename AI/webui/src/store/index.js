import { create } from 'zustand'
import * as api from '../api/client'

const useStore = create((set, get) => ({
  // ── Models ──────────────────────────────────────────────────────────────────
  models: [],
  selectedModelId: null,

  loadModels: async () => {
    const models = await api.getModels()
    set({ models })
    if (models.length > 0 && !get().selectedModelId) {
      set({ selectedModelId: models[0].id })
    }
  },

  addModel: async (modelPath, contextSize = 4096) => {
    const m = await api.addModel({ model_path: modelPath, context_size: contextSize })
    set(s => ({ models: [...s.models, m], selectedModelId: m.id }))
    return m
  },

  // ── Conversations ────────────────────────────────────────────────────────────
  conversations: [],       // [{ id, model_id, title, closed, dirty }]
  activeConvoId: null,
  messages: {},            // { [convoId]: [{ role, content }] }

  loadConversations: async () => {
    const convos = await api.getConversations()
    set({ conversations: convos })
  },

  newConversation: async (title = '') => {
    const { selectedModelId } = get()
    if (!selectedModelId) throw new Error('No model selected')
    const convo = await api.createConversation({
      model_id: selectedModelId,
      title: title || undefined,
    })
    set(s => ({
      conversations: [convo, ...s.conversations],
      activeConvoId: convo.id,
      messages: { ...s.messages, [convo.id]: [] },
    }))
    return convo
  },

  deleteConversation: async (id) => {
    await api.deleteConversation(id)
    set(s => ({
      conversations: s.conversations.filter(c => c.id !== id),
      activeConvoId: s.activeConvoId === id ? null : s.activeConvoId,
    }))
  },

  setActiveConvo: (id) => set({ activeConvoId: id }),

  sendMessage: async (convoId, text, temperature = 0.7, maxTokens = 2048) => {
    // Optimistically add user message.
    const userMsg = { role: 'user', content: text }
    set(s => ({
      messages: {
        ...s.messages,
        [convoId]: [...(s.messages[convoId] || []), userMsg],
      },
    }))
    // Add a pending assistant placeholder.
    const pending = { role: 'assistant', content: null }
    set(s => ({
      messages: {
        ...s.messages,
        [convoId]: [...(s.messages[convoId] || []), pending],
      },
    }))

    const res = await api.sendMessage(convoId, {
      message: text,
      temperature,
      max_tokens: maxTokens,
    })

    set(s => {
      const msgs = [...(s.messages[convoId] || [])]
      // Replace the last pending entry with the real reply.
      const lastIdx = msgs.length - 1
      msgs[lastIdx] = { role: 'assistant', content: res.reply }
      return { messages: { ...s.messages, [convoId]: msgs } }
    })
    return res.reply
  },

  // ── Agents ──────────────────────────────────────────────────────────────────
  agents: [],
  agentMessages: {},     // { [agentId]: [{ role, content }] }
  activeAgentId: null,

  loadAgents: async () => {
    const agents = await api.getAgents()
    set({ agents })
  },

  createAgent: async (data) => {
    const agent = await api.createAgent(data)
    set(s => ({ agents: [...s.agents, agent] }))
    return agent
  },

  deleteAgent: async (id) => {
    await api.deleteAgent(id)
    set(s => ({
      agents: s.agents.filter(a => a.id !== id),
      activeAgentId: s.activeAgentId === id ? null : s.activeAgentId,
    }))
  },

  setActiveAgent: (id) => set({ activeAgentId: id }),

  sendAgentMessage: async (agentId, text, temperature = 0.7, maxTokens = 2048) => {
    const userMsg = { role: 'user', content: text }
    set(s => ({
      agentMessages: {
        ...s.agentMessages,
        [agentId]: [...(s.agentMessages[agentId] || []), userMsg],
      },
    }))
    const pending = { role: 'assistant', content: null }
    set(s => ({
      agentMessages: {
        ...s.agentMessages,
        [agentId]: [...(s.agentMessages[agentId] || []), pending],
      },
    }))

    const res = await api.chatWithAgent(agentId, {
      message: text,
      temperature,
      max_tokens: maxTokens,
    })

    set(s => {
      const msgs = [...(s.agentMessages[agentId] || [])]
      const lastIdx = msgs.length - 1
      msgs[lastIdx] = { role: 'assistant', content: res.reply }
      return { agentMessages: { ...s.agentMessages, [agentId]: msgs } }
    })
    return res.reply
  },

  // ── UI ───────────────────────────────────────────────────────────────────────
  activePanel: 'chat',   // 'chat' | 'agents'
  setActivePanel: (p) => set({ activePanel: p }),

  error: null,
  setError: (e) => set({ error: e }),
  clearError: () => set({ error: null }),
}))

export default useStore
