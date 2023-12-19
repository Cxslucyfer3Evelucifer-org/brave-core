/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import styles from './style.module.scss'
import Button from '@brave/leo/react/button'
import Icon from '@brave/leo/react/icon'

interface CodeBlockProps {
  code: string
}

export function Inline(props: CodeBlockProps) {
  return (
    <span className={styles.container}>
      <code>
        {props.code}
      </code>
    </span>
  )
}

function Block(props: CodeBlockProps) {
  const [hasCopied, setHasCopied] = React.useState(false)

  const handleCopy = () => {
    navigator.clipboard.writeText(props.code).then(() => {
      setHasCopied(true)
      setTimeout(() => setHasCopied(false), 1000)
    })
  }

  return (
    <div className={styles.container}>
      <div className={styles.toolbar}>
        <div />
        <Button
          kind='plain-faint'
          onClick={handleCopy}
        >
          <div slot="icon-before">
            <Icon className={styles.icon} name={hasCopied ? 'check-circle-outline' : 'copy'} />
          </div>
          <div>Copy code</div>
        </Button>
      </div>
      <pre>{props.code}</pre>
    </div>
  )
}

const CodeBlock = {
  Inline,
  Block
}

export default CodeBlock
