import { useState, useRef, useEffect } from 'react'

function SendIcon() {
  return (
    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
      <line x1="22" y1="2" x2="11" y2="13"/>
      <polygon points="22 2 15 22 11 13 2 9 22 2"/>
    </svg>
  )
}

export default function ChatInput({ onSend, disabled, placeholder = 'Message…' }) {
  const [text, setText] = useState('')
  const taRef = useRef(null)

  const submit = () => {
    const val = text.trim()
    if (!val || disabled) return
    onSend(val)
    setText('')
  }

  const onKey = (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault()
      submit()
    }
  }

  // Auto-grow textarea
  useEffect(() => {
    const ta = taRef.current
    if (!ta) return
    ta.style.height = 'auto'
    ta.style.height = Math.min(ta.scrollHeight, 160) + 'px'
  }, [text])

  return (
    <div className="input-bar">
      <div className="input-wrap">
        <textarea
          ref={taRef}
          rows={1}
          placeholder={placeholder}
          value={text}
          onChange={e => setText(e.target.value)}
          onKeyDown={onKey}
          disabled={disabled}
        />
        <button className="btn-send" onClick={submit} disabled={disabled || !text.trim()}>
          <SendIcon />
        </button>
      </div>
    </div>
  )
}
