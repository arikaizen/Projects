import { useState } from 'react'
import useStore from '../store'

export default function CreateAgentModal({ onClose }) {
  const { models, selectedModelId, createAgent, setActiveAgent, setActivePanel, setError } = useStore()

  const [form, setForm] = useState({
    name: '',
    description: '',
    system_prompt: 'You are a helpful AI assistant.',
    model_id: selectedModelId ?? (models[0]?.id ?? ''),
  })
  const [saving, setSaving] = useState(false)

  const set = (k, v) => setForm(f => ({ ...f, [k]: v }))

  const handleSubmit = async () => {
    if (!form.name.trim()) return
    if (!form.system_prompt.trim()) return
    if (!form.model_id) return
    setSaving(true)
    try {
      const agent = await createAgent({
        name: form.name.trim(),
        description: form.description.trim(),
        system_prompt: form.system_prompt.trim(),
        model_id: Number(form.model_id),
      })
      setActiveAgent(agent.id)
      setActivePanel('agents')
      onClose()
    } catch (e) {
      setError(e.message)
    } finally {
      setSaving(false)
    }
  }

  return (
    <div className="modal-overlay" onClick={e => e.target === e.currentTarget && onClose()}>
      <div className="modal">
        <h2>Create Agent</h2>

        <div className="form-group">
          <label>Name *</label>
          <input
            placeholder="e.g. Code Reviewer"
            value={form.name}
            onChange={e => set('name', e.target.value)}
          />
        </div>

        <div className="form-group">
          <label>Description</label>
          <input
            placeholder="Short description (optional)"
            value={form.description}
            onChange={e => set('description', e.target.value)}
          />
        </div>

        <div className="form-group">
          <label>System Prompt *</label>
          <textarea
            rows={5}
            placeholder="Define the agent's role and behaviour…"
            value={form.system_prompt}
            onChange={e => set('system_prompt', e.target.value)}
          />
        </div>

        <div className="form-group">
          <label>Model *</label>
          <select value={form.model_id} onChange={e => set('model_id', e.target.value)}>
            {models.length === 0 && <option value="">No models loaded</option>}
            {models.map(m => (
              <option key={m.id} value={m.id}>
                {m.path.split('/').pop()} (#{m.id})
              </option>
            ))}
          </select>
        </div>

        <div className="modal-actions">
          <button className="btn btn-secondary" onClick={onClose}>Cancel</button>
          <button
            className="btn btn-primary"
            onClick={handleSubmit}
            disabled={saving || !form.name.trim() || !form.model_id}
          >
            {saving ? <span className="spinner" style={{ width: 12, height: 12 }} /> : 'Create Agent'}
          </button>
        </div>
      </div>
    </div>
  )
}
