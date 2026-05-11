const BASE = '/api'

async function req(method, path, body) {
  const opts = {
    method,
    headers: { 'Content-Type': 'application/json' },
  }
  if (body !== undefined) opts.body = JSON.stringify(body)
  const res = await fetch(BASE + path, opts)
  if (!res.ok) {
    const err = await res.json().catch(() => ({ error: res.statusText }))
    throw new Error(err.error || res.statusText)
  }
  if (res.status === 204) return null
  return res.json()
}

// Models
export const getModels       = ()      => req('GET',    '/models')
export const addModel        = (data)  => req('POST',   '/models', data)

// Conversations
export const getConversations = ()     => req('GET',    '/conversations')
export const createConversation = (d)  => req('POST',   '/conversations', d)
export const deleteConversation = (id) => req('DELETE', `/conversations/${id}`)
export const sendMessage      = (id, d)=> req('POST',   `/conversations/${id}/chat`, d)

// Agents
export const getAgents        = ()     => req('GET',    '/agents')
export const createAgent      = (data) => req('POST',   '/agents', data)
export const deleteAgent      = (id)   => req('DELETE', `/agents/${id}`)
export const chatWithAgent    = (id, d)=> req('POST',   `/agents/${id}/chat`, d)
