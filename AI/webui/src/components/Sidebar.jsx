import { useState } from 'react'
import useStore from '../store'

function BotIcon() {
  return (
    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="3" y="11" width="18" height="10" rx="2"/>
      <path d="M12 11V5"/>
      <circle cx="12" cy="4" r="1"/>
      <path d="M8 15h.01M12 15h.01M16 15h.01"/>
    </svg>
  )
}

function PlusIcon() {
  return (
    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
      <line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>
    </svg>
  )
}

function ChatIcon() {
  return (
    <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
      <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>
    </svg>
  )
}

function AgentIcon() {
  return (
    <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
      <circle cx="12" cy="8" r="4"/>
      <path d="M6 20v-2a6 6 0 0 1 12 0v2"/>
      <path d="M19 13l2 2-2 2"/>
    </svg>
  )
}

export default function Sidebar() {
  const {
    models, selectedModelId, addModel, loadModels,
    conversations, activeConvoId, setActiveConvo,
    newConversation, deleteConversation,
    agents, activeAgentId, setActiveAgent,
    activePanel, setActivePanel,
    setError,
  } = useStore()

  const [newModelPath, setNewModelPath] = useState('')
  const [addingModel, setAddingModel] = useState(false)

  const handleAddModel = async () => {
    if (!newModelPath.trim()) return
    setAddingModel(true)
    try {
      await addModel(newModelPath.trim())
      setNewModelPath('')
      await loadModels()
    } catch (e) {
      setError(e.message)
    } finally {
      setAddingModel(false)
    }
  }

  const handleNewChat = async () => {
    try {
      setActivePanel('chat')
      await newConversation()
    } catch (e) {
      setError(e.message)
    }
  }

  const openConvo = (id) => {
    setActivePanel('chat')
    setActiveConvo(id)
  }

  const openAgent = (id) => {
    setActivePanel('agents')
    setActiveAgent(id)
  }

  return (
    <aside className="sidebar">
      <div className="sidebar-header">
        <div className="sidebar-logo">
          <BotIcon />
          AI Studio
        </div>
        <button className="btn-new-chat" onClick={handleNewChat}>
          <PlusIcon /> New Chat
        </button>
      </div>

      <div className="sidebar-nav">
        <button
          className={`nav-tab ${activePanel === 'chat' ? 'active' : ''}`}
          onClick={() => setActivePanel('chat')}
        >
          Chats
        </button>
        <button
          className={`nav-tab ${activePanel === 'agents' ? 'active' : ''}`}
          onClick={() => setActivePanel('agents')}
        >
          Agents
        </button>
      </div>

      <div className="sidebar-list">
        {activePanel === 'chat' && (
          <>
            <div className="sidebar-section-label">Conversations</div>
            {conversations.length === 0 && (
              <div style={{ padding: '12px 6px', color: 'var(--text-muted)', fontSize: 12 }}>
                No chats yet. Click "New Chat".
              </div>
            )}
            {conversations.map(c => (
              <div
                key={c.id}
                className={`sidebar-item ${activeConvoId === c.id ? 'active' : ''}`}
                onClick={() => openConvo(c.id)}
              >
                <ChatIcon />
                <span className="sidebar-item-label">{c.title || `Chat ${c.id}`}</span>
                <button
                  className="sidebar-item-del"
                  title="Delete"
                  onClick={e => { e.stopPropagation(); deleteConversation(c.id) }}
                >
                  ✕
                </button>
              </div>
            ))}
          </>
        )}

        {activePanel === 'agents' && (
          <>
            <div className="sidebar-section-label">Agents</div>
            {agents.length === 0 && (
              <div style={{ padding: '12px 6px', color: 'var(--text-muted)', fontSize: 12 }}>
                No agents yet. Create one →
              </div>
            )}
            {agents.map(a => (
              <div
                key={a.id}
                className={`sidebar-item ${activeAgentId === a.id ? 'active' : ''}`}
                onClick={() => openAgent(a.id)}
              >
                <AgentIcon />
                <span className="sidebar-item-label">{a.name}</span>
                <button
                  className="sidebar-item-del"
                  title="Delete"
                  onClick={e => { e.stopPropagation(); useStore.getState().deleteAgent(a.id) }}
                >
                  ✕
                </button>
              </div>
            ))}
          </>
        )}
      </div>

      <div className="add-model-row">
        <input
          placeholder="Path to .gguf model…"
          value={newModelPath}
          onChange={e => setNewModelPath(e.target.value)}
          onKeyDown={e => e.key === 'Enter' && handleAddModel()}
        />
        <button onClick={handleAddModel} disabled={addingModel}>
          {addingModel ? <span className="spinner" /> : 'Load'}
        </button>
      </div>

      <div className="sidebar-footer">
        <div className="model-select-label">Active model</div>
        <select
          className="model-select"
          value={selectedModelId ?? ''}
          onChange={e => useStore.setState({ selectedModelId: Number(e.target.value) })}
        >
          {models.length === 0 && <option value="">No models loaded</option>}
          {models.map(m => (
            <option key={m.id} value={m.id}>
              {m.path.split('/').pop()} (#{m.id})
            </option>
          ))}
        </select>
      </div>
    </aside>
  )
}
