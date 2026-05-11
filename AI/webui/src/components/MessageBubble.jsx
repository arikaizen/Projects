import ReactMarkdown from 'react-markdown'
import remarkGfm from 'remark-gfm'

export default function MessageBubble({ role, content }) {
  const isUser = role === 'user'
  const isPending = content === null

  return (
    <div className={`message ${isUser ? 'user' : ''}`}>
      <div className={`avatar ${isUser ? 'user' : 'assistant'}`}>
        {isUser ? 'U' : '🤖'}
      </div>
      <div className={`bubble ${isUser ? 'user' : 'assistant'} ${isPending ? 'pending' : ''}`}>
        {isPending ? (
          <span className="spinner" />
        ) : isUser ? (
          content
        ) : (
          <ReactMarkdown remarkPlugins={[remarkGfm]}>{content}</ReactMarkdown>
        )}
      </div>
    </div>
  )
}
