# Orb behavior script
fn on_update(dt)
  rotate(0, 120, 0)
end

fn on_trigger(tag)
  add_score(1)
  play_sound("chime")
  destroy_self()
end
