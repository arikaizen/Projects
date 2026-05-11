import { useEffect, useRef, useState } from 'react'
import useStore from '../store'
import MessageBubble from './MessageBubble'
import ChatInput from './ChatInput'
import CreateAgentModal from './CreateAgentModal'

function PlusIcon() {
  return (
    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
      <line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>
    </svg>
  )
}

export default function AgentsPanel() {
  const {
    agents, activeAgentId, setActiveAgent,
    agentMessages, sendAgentMessage, deleteAgent,
    models, setError,
  } = useStore()

  const [creating, setCreating] = useState(false)
  const [sending, setSending] = useState(false)
  const bottomRef = useRef(null)

  const agent = agents.find(a => a.id === activeAgentId)
  const msgs = agentMessages[activeAgentId] || []

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [msgs.length])

  const handleSend = async (text) => {
    if (!activeAgentId) return
    setSending(true)
    try {
      await sendAgentMessage(activeAgentId, text)
    } catch (e) {
      setError(e.message)
    } finally {
      setSending(false)
    }
  }

  const modelName = (mid) => {
    const m = models.find(m => m.id === mid)
    return m ? m.path.split('/').pop() : `Model #${mid}`
  }

  return (
    <div className="agents-panel">
      <div className="topbar">
        <span className="topbar-title">
          {agent ? agent.name : 'Agents'}
        </span>
        <button
          className="btn btn-primary"
          style={{ padding: '6px 14px', fontSize: 12 }}
          onClick={() => setCreating(true)}
        >
          <PlusIcon /> New Agent
        </button>
      </div>

      <div className="agents-grid">
        {/* Left: agent list */}
        <div className="agents-list-pane">
          <div className="agents-list-header">
            <span>My Agents</span>
            <span style={{ color: 'var(--text-muted)', fontSize: 11 }}>{agents.length}</span>
          </div>
          <div className="agents-list-scroll">
            {agents.length === 0 && (
              <div style={{ padding: '16px 12px', color: 'var(--text-muted)', fontSize: 12, lineHeight: 1.5 }}>
                No agents yet.<br />Click "New Agent" to create one.
              </div>
            )}
            {agents.map(a => (
              <div
                key={a.id}
                className={`agent-card ${activeAgentId === a.id ? 'active' : ''}`}
                onClick={() => setActiveAgent(a.id)}
              >
                <div className="agent-card-name">{a.name}</div>
                {a.description && (
                  <div className="agent-card-desc">{a.description}</div>
                )}
                <div style={{ fontSize: 11, color: 'var(--text-muted)', marginTop: 4 }}>
                  {modelName(a.model_id)}
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Right: chat with agent */}
        <div className="agents-chat-pane">
          {!agent ? (
            <div className="empty-state">
              <div className="empty-state-icon">🤖</div>
              <h2>Select an Agent</h2>
              <p>Pick an agent from the list, or create a new one to get started.</p>
              <button
                className="btn btn-primary"
                style={{ marginTop: 8 }}
                onClick={() => setCreating(true)}
              >
                <PlusIcon /> New Agent
              </button>
            </div>
          ) : (
            <>
              <div className="agent-info-bar">
                <span>🤖</span>
                <span><strong>{agent.name}</strong></span>
                <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  — {agent.system_prompt.slice(0, 80)}{agent.system_prompt.length > 80 ? '…' : ''}
                </span>
                <button
                  className="btn-icon"
                  title="Delete agent"
                  onClick={() => deleteAgent(agent.id).catch(e => setError(e.message))}
                >
                  🗑
                </button>
              </div>

              <div className="messages">
                {msgs.length === 0 && (
                  <div className="empty-state" style={{ flex: 'none', paddingTop: 48 }}>
                    <div className="empty-state-icon" style={{ fontSize: 32 }}>💬</div>
                    <p>Give <strong>{agent.name}</strong> a task to get started.</p>
                  </div>
                )}
                {msgs.map((m, i) => (
                  <MessageBubble key={i} role={m.role} content={m.content} />
                ))}
                <div ref={bottomRef} />
              </div>

              <ChatInput
                onSend={handleSend}
                disabled={sending}
                placeholder={`Give ${agent.name} a task…`}
              />
            </>
          )}
        </div>
      </div>

      {creating && <CreateAgentModal onClose={() => setCreating(false)} />}
    </div>
  )
}
