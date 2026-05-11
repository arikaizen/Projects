import { useEffect, useRef, useState } from 'react'
import useStore from '../store'
import MessageBubble from './MessageBubble'
import ChatInput from './ChatInput'

export default function ChatView() {
  const {
    conversations, activeConvoId, messages,
    sendMessage, newConversation, selectedModelId,
    setError,
  } = useStore()

  const [sending, setSending] = useState(false)
  const bottomRef = useRef(null)

  const convo = conversations.find(c => c.id === activeConvoId)
  const msgs = messages[activeConvoId] || []

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [msgs.length])

  const handleSend = async (text) => {
    let cid = activeConvoId
    setSending(true)
    try {
      if (!cid) {
        if (!selectedModelId) throw new Error('No model loaded — add a .gguf model first.')
        const c = await newConversation()
        cid = c.id
      }
      await sendMessage(cid, text)
    } catch (e) {
      setError(e.message)
    } finally {
      setSending(false)
    }
  }

  if (!activeConvoId && conversations.length === 0) {
    return (
      <div className="chat-area">
        <div className="empty-state">
          <div className="empty-state-icon">🤖</div>
          <h2>AI Studio</h2>
          <p>Load a .gguf model using the input at the bottom-left, then start a new chat.</p>
        </div>
        <ChatInput onSend={handleSend} disabled={sending} placeholder="Start typing to begin a new chat…" />
      </div>
    )
  }

  return (
    <div className="chat-area">
      <div className="topbar">
        <span className="topbar-title">
          {convo?.title || (activeConvoId ? `Chat ${activeConvoId}` : 'New Chat')}
        </span>
        {convo && (
          <span className="topbar-badge">
            {conversations.find(c => c.id === activeConvoId)?.model_id
              ? `Model #${conversations.find(c => c.id === activeConvoId).model_id}`
              : ''}
          </span>
        )}
      </div>

      <div className="messages">
        {msgs.length === 0 && (
          <div className="empty-state" style={{ flex: 'none', paddingTop: 60 }}>
            <div className="empty-state-icon" style={{ fontSize: 32 }}>💬</div>
            <p>Send a message to start the conversation.</p>
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
        placeholder="Message the AI…"
      />
    </div>
  )
}
